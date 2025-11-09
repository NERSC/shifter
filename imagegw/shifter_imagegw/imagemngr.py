#!/usr/bin/python
# Shifter, Copyright (c) 2015, The Regents of the University of California,
# through Lawrence Berkeley National Laboratory (subject to receipt of any
# required approvals from the U.S. Dept. of Energy).  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. Neither the name of the University of California, Lawrence Berkeley
#     National Laboratory, U.S. Dept. of Energy nor the names of its
#     contributors may be used to endorse or promote products derived from this
#     software without specific prior written permission.`
#
# See LICENSE for full text.

"""
Image Manager for the Shifter Gateway.

This module is provides the interface layer for the image manager.  This
compliments the api module which provides the REST interface.  This module
does much of the heavy lifting for the image manager.  It handles all
interactions with the Mongo Database and dispatches work through a thread pool.
"""

import os
import logging
from time import time, sleep
from pymongo import MongoClient
import pymongo.errors
from shifter_imagegw.imageworker import WorkerThreads
from shifter_imagegw.imageworker import PullRequest
from shifter_imagegw.imageworker import ImportRequest
from shifter_imagegw.imageworker import ExpireRequest
from shifter_imagegw.models import Session
from shifter_imagegw.config import Config
import grp
from multiprocessing import Process
import atexit
from cachetools import cached, TTLCache, LFUCache


# decorator function to re-attempt any mongo operation that may have failed
# owing to AutoReconnect (e.g., mongod coming back, etc).  This may increase
# the opportunity for race conditions, and should be more closely considered
# for the insert/update functions
def mongo_reconnect_reattempt(call):
    """Automatically re-attempt potentially failed mongo operations"""
    def _mongo_reconnect_safe(self, *args, **kwargs):
        for _ in range(2):
            try:
                return call(self, *args, **kwargs)
            except pymongo.errors.AutoReconnect:
                logging.warning("Error: mongo reconnect attmempt")
                sleep(2)
        logging.warning("Error: Failed to deal with mongo auto-reconnect!")
        raise ConnectionError('Reconnect to mongo failed')
    return _mongo_reconnect_safe


class ImageMngr(object):
    """
    This class handles most of the backend work for the image gateway.
    It uses a Mongo Database to track state, uses threads to dispatch work,
    and has public functions to lookup, pull and expire images.
    """

    def __init__(self, config: Config):
        """
        Create an instance of the image manager.
        """

        logging.debug('Initializing image manager')
        # Time before another pull can be attempted
        self.pullupdatetimeout = 300
        self.pullupdatetimeout = config.PullUpdateTimeout
        # Max amount of time to allow for a pull
        self.pulltimeout = self.pullupdatetimeout
        # This is not intended to provide security, but just
        # provide a basic check that a session object is correct
        self.magic = 'imagemngrmagic'

        # Connect to database
        threads = config.WorkerThreads
        self.workers = WorkerThreads(config, threads=threads)
        self.status_queue = self.workers.get_updater_queue()
        self.status_proc = Process(target=self.status_thread,
                                   kwargs={"config": config},
                                   name='StatusThread')
        self.status_proc.start()
        atexit.register(self.shutdown)
        self.mongo_init(config)
        # Cleanup any pending requests
        self._images_remove_many({'status': 'PENDING'})
        self.platforms = config.Platforms
        self.systems = config.Platforms.keys()
        self.config = config

    def shutdown(self):
        logging.info("Shutdown called")
        self.status_queue.put('stop')
        self.status_proc.terminate()

    def mongo_init(self, config: Config):
        client = MongoClient(config.MongoDBURI)
        db_ = config.MongoDB
        self.images = client[db_].images
        self.metrics = None
        if config.Metrics:
            self.metrics = client[db_].metrics

    def status_thread(self, config: Config | None = None):
        """
        This listens for update messages from a queue.
        """
        self.mongo_init(config)
        while True:
            message = self.status_queue.get()
            if message == 'stop':
                logging.info("Shutting down Status Thread")
                break
            ident = message['id']
            state = message['state']
            meta = message['meta']
            # TODO: Handle a failed expire
            if state == "FAILURE":
                logging.warning(f"Operation failed for {ident}")

            # A response message
            if state != 'READY':
                self.update_mongo_state(ident, state, meta)
                continue
            if 'response' in meta and meta['response']:
                response = meta['response']
                logging.debug(response)
                if 'meta_only' in response:
                    logging.debug('Updating ACLs')
                    self.update_acls(ident, response)
                else:
                    self.complete_pull(ident, response)
                logging.debug('meta={str(response)}')

    def _isadmin(self, session: Session, system: str | None = None):
        """
        Check if this is an admin user.
        Returns true if is an admin or false if not.
        """
        if not self.platforms[system].admins:
            return False
        admins = self.platforms[system].admins
        user = session.user
        if user in admins:
            logging.debug(f'user {user} is an admin')
            return True
        return False

    def _isasystem(self, system: str):
        """Check if system is a valid platform."""
        return bool(system in self.systems)

    def _checkread(self, session: Session, rec: dict):
        """
        Checks if the user has read permissions to the image.
        """

        # Start by checking if the image is public (no ACLs)
        if 'private' in rec and rec['private'] is False:
            return True
        iUACL = None
        iGACL = None
        if 'userACL' in rec:
            iUACL = rec['userACL']
        if 'groupACL' in rec:
            iGACL = rec['groupACL']
        if iUACL is None and iGACL is None:
            return True
        if iUACL == [] and iGACL == []:
            return True
        uid = session.uid
        gid = session.gid
        logging.debug(f'uid={uid} iUACL={str(iUACL)}')
        logging.debug('sessions = ' + str(session))
        if iUACL is not None and uid in iUACL:
            return True
        if iGACL is not None and gid in iGACL:
            return True
        if iGACL:
            for group in iGACL:
                members = grp.getgrgid(group).gr_mem
                if session.user in members:
                    return True
        return False

    def _resetexpire(self, ident: str):
        """Reset the expire time.  (Not fully implemented)."""
        # Change expire time for image
        # TODO shore up expire-time parsing
        expire_timeout = self.config.ImageExpirationTimeout
        (days, hours, minutes, secs) = expire_timeout.split(':')
        expire = time() + int(secs) + 60 * (int(minutes) +
                                            60 * (int(hours) + 24 * int(days)))
        self._images_update({'_id': ident}, {'$set': {'expiration': expire}})
        return expire

    def _make_acl(self, acllist: dict, id: str):
        if id not in acllist:
            acllist.append(id)
        return acllist

    def _compare_list(self, a: dict, b: dict, key: str):
        """"
        look at the key element of two objects
        and compare the list of ids.

        return True if everything matches
        return False if anything is different
        """

        # If the key isn't in the objects or
        # something else fails, then it must
        # have changed.
        try:
            if key not in a:
                return False
            if key not in b:
                return False
        except Exception:
            return True
        aitems = a[key]
        bitems = b[key]
        if len(aitems) != len(bitems):
            return False
        for item in aitems:
            if item not in bitems:
                return False
        return True

    @cached(cache=LFUCache(maxsize=128), info=True)
    def _add_metrics(self,
                     session: Session,
                     system: str,
                     itype: str,
                     tag: str,
                     id: str):
        """
        Adds a metric record for a lookup.  The caching
        avoids recording the same information reqpeatedly,
        since we don't care about the number of lookups
        for the same image and person.
        """
        try:
            r = {
                'user': session.user,
                'uid': session.uid,
                'system': system,
                'type': itype,
                'tag': tag,
                'id': id,
                'time': time()
            }
            self._metrics_insert(r)
        except Exception:
            logging.warning('Failed to log lookup.')

    def get_metrics(self, session: Session, system: str, limit: int):
        """
        Return the last <limit> lookup records.
        """
        recs = []
        if not self._isadmin(session, system):
            return recs
        if self.metrics is None:
            return recs
        count = self.metrics.count_documents({})
        skip = count - limit
        if skip < 0:
            skip = 0
        for r in self.metrics.find().skip(skip):
            r.pop('_id', None)
            recs.append(r)
        return recs

    @cached(cache=TTLCache(maxsize=100, ttl=60), info=True)
    def lookup(self,
               session: Session,
               system: str,
               itype: str,
               tag: str
               ):
        """
        Lookup an image.
        Image is dictionary with system,itype and tag defined.
        """
        query = {
            'status': 'READY',
            'system': system,
            'itype': itype,
            'tag': {'$in': [tag]}
        }
        self.update_states()
        rec = self._images_find_one(query)
        if rec:
            if self._checkread(session, rec) is False:
                return None
            self._resetexpire(rec['_id'])

        if rec and self.metrics is not None:
            self._add_metrics(session,
                              system,
                              itype,
                              tag,
                              rec['id'])
        return rec

    def imglist(self, session: Session, system: str):
        """
        list images for a system.
        Image is dictionary with system defined.
        """
        if self._isasystem(system) is False:
            raise OSError("Invalid System")
        query = {'status': 'READY', 'system': system}
        self.update_states()
        records = self._images_find(query)
        resp = []
        for record in records:
            if self._checkread(session, record):
                resp.append(record)
        # verify access
        return resp

    def show_queue(self, session: Session, system: str):
        """
        list queue for a system.
        Image is dictionary with system defined.
        """
        query = {'status': {'$ne': 'READY'}, 'system': system}
        self.update_states()
        records = self._images_find(query)
        resp = []
        for record in records:
            if 'pulltag' not in record:
                continue
            if record['status'] == 'EXPIRED':
                continue
            resp.append({'status': record['status'],
                        'image': record['pulltag']})
        return resp

    def _pullable(self, rec: dict):
        """
        An image is pullable when:
        -There is no existing record
        -The status is a FAILURE
        -The status is READY and it is past the update time
        -The state is something else and the pull has expired
        """

        # if rec is None then do a pull
        if rec is None:
            return True

        # Okay there has been a pull before
        # If the status flag is missing just repull (shouldn't happen)
        if 'status' not in rec:
            return True
        status = rec['status']

        # EXPIRED images can be pulled
        if status == 'EXPIRED':
            return True

        # Need to deal with last_pull for a READY record
        if 'last_pull' not in rec:
            return True
        nextpull = self.pullupdatetimeout + rec['last_pull']

        # It has been a while, so re-pull to see if it is fresh
        if status == 'READY' and (time() > nextpull):
            return True

        # Repull failed pulls
        if status == 'FAILURE' and (time() > nextpull):
            return True

        # Last thing... What if the pull somehow got hung or died in the middle
        # See if heartbeat is old
        # TODO: add pull timeout.  For now use 1 hour
        if status != 'READY' and 'last_heartbeat' in rec:
            if (time() - rec['last_heartbeat']) > 3600:
                return True

        return False

    def new_pull_record(self, image: dict):
        """
        Creates a new image in mongo.  If the pull already exist it removes
        it first.
        """
        # Clean out any existing records
        for rec in self._images_find(image):
            if rec['status'] == 'READY':
                continue
            else:
                self._images_remove({'_id': rec['_id']})
        newimage = {
            'format': 'invalid',  # <ext4|squashfs|vfs>
            'userACL': [],
            'groupACL': [],
            'private': None,
            'tag': [],
            'status': 'INIT'
        }
        newimage['format'] = self.config.DefaultImageFormat
        for param in image:
            if param == 'tag':
                continue
            newimage[param] = image[param]
        self._images_insert(newimage)
        return newimage

    def pull(self, session: Session, image: dict):
        """
        pull the image
        Takes an auth token, a request object
        """
        request = {
            'system': image['system'],
            'itype': image['itype'],
            'pulltag': image['tag']
        }
        # If a pull request exist for this tag
        #  check to see if it is expired or a failure, if so remove it
        # otherwise
        #  return the record
        rec = None
        # find any pull record
        self.update_states()
        # let's lookup the active image
        query = {
            'status': 'READY',
            'system': image['system'],
            'itype': image['itype'],
            'tag': {'$in': [image['tag']]}
        }
        rec = self._images_find_one(query)
        for record in self._images_find(request):
            status = record['status']
            if status == 'READY' or status == 'SUCCESS':
                continue
            rec = record
            break
        inflight = False
        recent = False
        if rec is not None and rec['status'] != 'READY':
            inflight = True
        elif rec is not None:
            # if an image has been pulled in the last 60 seconds
            # let's consider that "recent"
            if (time() - rec['last_pull']) < 10:
                recent = True
        request['userACL'] = []
        request['groupACL'] = []
        if 'userACL' in image and image['userACL'] != []:
            request['userACL'] = self._make_acl(image['userACL'],
                                                session.uid)
        if 'groupACL' in image and image['groupACL'] != []:
            request['groupACL'] = self._make_acl(image['groupACL'],
                                                 session.gid)
        if self._compare_list(request, rec, 'userACL') and \
                self._compare_list(request, rec, 'groupACL'):
            acl_changed = False
        else:
            logging.debug("No ACL change detected.")
            acl_changed = True

        # We could hit a key error or some other edge case
        # so just do our best and update if there are problems
        update = False
        if not recent and not inflight and acl_changed:
            logging.debug("ACL change detected.")
            update = True

        if self._pullable(rec):
            logging.debug("Pullable image")
            update = True

        if update:
            logging.debug("Creating New Pull Record")
            rec = self.new_pull_record(request)
            ident = rec['_id']
            logging.debug("PENDING Request")
            self.update_mongo_state(ident, 'PENDING')
            request['tag'] = request['pulltag']
            request['session'] = session
            logging.debug("Calling do pull with queue="
                          f"{request['system']}")
            pr = PullRequest(self.config,
                             session.system,
                             request['tag'],
                             ident,
                             session,
                             useracl=request['userACL'],
                             groupacl=request['groupACL'])
            self.workers.submit(pr)

            memo = "pull request queued " \
                   f"s={request['system']} tag={request['tag']}"
            logging.info(memo)

            self.update_mongo(ident, {'last_pull': time()})

        return rec

    def mngrimport(self, session: Session, image: dict):
        """
        import the image directly from a file
        Only for allowed users
        Takes an auth token, a request object
        """
        meta = {}
        fp = image['filepath']
        request = {
            'system': image['system'],
            'itype': image['itype'],
            'pulltag': image['tag'],
            'filepath': image['filepath'],
            'format': image['format'],
            'meta': meta
        }
        logging.debug(f'mngrmport called for file {fp}')
        # Skip checks about previous requests for now
        # Future work could check the fasthash and
        # not import if they're the same
        q = {
            'system': image['system'],
            'itype': image['itype'],
            'pulltag': image['tag']
        }
        rec = self._images_find_one(q)
        if not self._pullable(rec):
            return rec

        # We could hit a key error or some other edge case
        # so just do our best and update if there are problems

        logging.debug("Creating New Import Record")
        # new_pull_record works for import too
        rec = self.new_pull_record(request)
        ident = rec['_id']
        logging.debug(f"PENDING Request, ident {ident}")
        self.update_mongo_state(ident, 'PENDING')
        request['tag'] = request['pulltag']
        request['session'] = session
        logging.debug("Calling wrkimport with queue="
                      "{request['system']}")
        ir = ImportRequest(self.config,
                           session.system,
                           image['tag'],
                           ident,
                           session,
                           image['filepath'])
        self.workers.submit(ir)

        memo = "import request queued " \
               f"s={request['system']} tag={request['tag']}"
        logging.info(memo)
        self.update_mongo(ident, {'last_pull': time()})
        return rec

    def update_mongo_state(self, ident: str, state: str, info: dict | None = None):
        """
        Helper function to set the mongo state for an image with _id==ident
        to state=state.
        """
        if state == 'SUCCESS':
            state = 'READY'
        set_list = {'status': state, 'status_message': ''}
        if info is not None and isinstance(info, dict):
            if 'heartbeat' in info:
                set_list['last_heartbeat'] = info['heartbeat']
            if 'message' in info:
                set_list['status_message'] = info['message']
        self._images_update({'_id': ident}, {'$set': set_list})

    def add_tag(self, ident: str, system: str, tag):
        """
        Helper function to add a tag to an image.
        ident is the mongo id (not image id)
        """
        # Remove the tag first
        self.remove_tag(system, tag)
        # see if tag isn't a list
        rec = self._images_find_one({'_id': ident})
        if rec is not None and 'tag' in rec and \
                not isinstance(rec['tag'], (list)):
            memo = f'Fixing tag for non-list {ident} {str(rec["tag"])}'
            logging.info(memo)
            curtag = rec['tag']
            self._images_update({'_id': ident}, {'$set': {'tag': [curtag]}})
        self._images_update({'_id': ident}, {'$addToSet': {'tag': tag}})
        return True

    def remove_tag(self, system: str, tag: str):
        """
        Helper function to remove a tag to an image.
        """
        self._images_update_many({'system': system, 'tag': {'$in': [tag]}},
                                 {'$pull': {'tag': tag}})
        return True

    def update_acls(self, ident: str, response: dict):
        logging.debug(f"Update ACLs called for {ident} {str(response)}")
        pullrec = self._images_find_one({'_id': ident})
        if pullrec is None:
            logging.error('ERROR: Missing pull request resp=',
                          f'{str(response)}')
            return
        # Check that this image ident doesn't already exist for this system
        rec = self._images_find_one({'id': response['id'], 'status': 'READY',
                                    'system': pullrec['system']})
        if rec is None:
            # This means the image already existed, but we didn't have a
            # record of it.  That seems odd (it happens in tests).  Let's
            # note it and power on through.
            msg = "WARNING: No image record found for an ACL update"
            logging.warning(msg)
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.update_mongo(ident, response)
            self.add_tag(ident, pullrec['system'], pullrec['pulltag'])
        else:
            self.add_tag(rec['_id'], pullrec['system'], pullrec['pulltag'])
            updates = {
                'userACL': response['userACL'],
                'groupACL': response['groupACL'],
                'private': response['private'],
                'last_pull': time()
            }
            logging.debug("Doing ACLs update")
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.update_mongo(rec['_id'], updates)
            self._images_remove({'_id': ident})

    def complete_pull(self, ident: str, response: dict):
        """
        Transition a completed pull request to an available image.
        """

        logging.debug(f"Complete called for {ident} {str(response)}")
        pullrec = self._images_find_one({'_id': ident})
        if pullrec is None:
            logging.warning(f'Missing pull request resp={str(response)}')
            return
        # Check that this image ident doesn't already exist for this system
        rec = self._images_find_one({'id': response['id'],
                                     'system': pullrec['system'],
                                     'status': 'READY'})
        tag = pullrec['pulltag']
        if rec is not None:
            # So we already had this image.
            # Let's delete the pull record.
            # TODO: update the pull time of the matching id
            logging.warning('Duplicate image')
            update_rec = {
                'last_pull': time()
            }
            self.update_mongo(rec['_id'], update_rec)

            self._images_remove({'_id': ident})
            # However it could be a new tag.  So let's update the tag
            try:
                rec['tag'].index(response['tag'])
            except ValueError:
                self.add_tag(rec['_id'], pullrec['system'], tag)
            return True
        else:
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.update_mongo(ident, response)
            self.add_tag(ident, pullrec['system'], tag)

    def update_mongo(self, ident: str, resp: dict):
        """
        Helper function to set the mongo values for an image with _id==ident.
        """
        setline = dict()
        # This maps from the key name in the response to the
        # key name used in mongo
        mappings = {
            'id': 'id',
            'entrypoint': 'ENTRY',
            'env': 'ENV',
            'workdir': 'WORKDIR',
            'last_pull': 'last_pull',
            'userACL': 'userACL',
            'groupACL': 'groupACL',
            'private': 'private',
            'status': 'status'
        }
        if os.environ.get('ENABLE_LABELS'):
            mappings['labels'] = 'LABELS'

        if 'private' in resp and resp['private'] is False:
            resp['userACL'] = []
            resp['groupACL'] = []

        for key in list(mappings.keys()):
            if key in resp:
                setline[mappings[key]] = resp[key]

        self._images_update({'_id': ident}, {'$set': setline})

    def get_state(self, ident: str):
        """
        Lookup the state of the image with _id==ident in Mongo.
        Returns the state.
        """
        self.update_states()
        rec = self._images_find_one({'_id': ident}, {'status': 1})
        if rec is None:
            return None
        elif 'status' not in rec:
            return None
        return rec['status']

    def update_states(self):
        """
        Cleanup failed transcations after a period
        """
        for rec in self._images_find({'status': 'FAILURE'}):
            nextpull = self.pullupdatetimeout + rec['last_pull']
            # It it has been a while then let's clean up
            if time() > nextpull:
                self._images_remove({'_id': rec['_id']})

    def autoexpire(self, session: Session, system: str):
        """Auto expire images and do cleanup"""
        # While this should be safe, let's restrict this to admins
        if not self._isadmin(session, system):
            return False
        # Cleanup - Lookup for things stuck in non-READY state
        self.update_states()
        removed = []
        for rec in self._images_find({'status': {'$ne': 'READY'},
                                     'system': system}):
            if 'last_pull' not in rec:
                logging.warning('Image missing last_pull for pulltag:' +
                                rec['pulltag'])
                continue
            if time() > rec['last_pull'] + self.pulltimeout:
                removed.append(rec['_id'])
                self._images_remove({'_id': rec['_id']})

        expired = []
        # Look for READY images that haven't been pulled recently
        for rec in self._images_find({'status': 'READY', 'system': system}):
            if 'expiration' not in rec:
                continue
            elif rec['expiration'] < time():
                logging.debug(f"expiring {rec['id']}")
                ident = rec.pop('_id')
                self.expire_id(rec, ident)
                if 'id' in rec:
                    expired.append(rec['id'])
                else:
                    expired.append('unknown')
            logging.debug(f"expired: {rec['expiration'] > time()}")
        return expired

    def expire_id(self, rec: dict, ident: str):
        """ Helper function to expire by id """
        memo = f"Calling do expire id={ident}"
        logging.debug(memo)
        logging.debug(f"Removing {rec['system']} {rec['id']}")
        er = ExpireRequest(self.config,
                           rec['system'],
                           rec['tag'],
                           rec['id'],
                           ident)

        self.workers.submit(er)
        logging.info("expire request queued "
                     f"s={rec['system']} tag={ident}")

    def expire(self, session: Session, image: dict):
        """Expire an image.  (Not Implemented)"""
        if not self._isadmin(session, image['system']):
            return False
        query = {
            'system': image['system'],
            'itype': image['itype'],
            'tag': {'$in': [image['tag']]}
        }
        rec = self._images_find_one(query)
        if rec is None:
            return None
        ident = rec.pop('_id')
        memo = "Calling do expire with " \
               f"queue={image['system']} id={ident}"
        logging.debug(memo)
        er = ExpireRequest(self.config,
                           rec['system'],
                           rec['tag'],
                           rec['id'],
                           ident)

        self.workers.submit(er)

        memo = "expire request queued " \
               f"s={image['system']} tag={image['tag']}"
        logging.info(memo)

        return True

    def __getstate__(self):
        """
        This is so the state can be serialized
        """
        self_dict = self.__dict__.copy()
        del self_dict['workers']
        return self_dict

    def __setstate__(self, state: str):
        self.__dict__.update(state)

    @mongo_reconnect_reattempt
    def _images_remove(self, *args, **kwargs):
        """ Decorated function to remove images from mongo """
        return self.images.delete_one(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _images_remove_many(self, *args, **kwargs):
        """ Decorated function to remove images from mongo """
        return self.images.delete_many(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _images_update(self, *args, **kwargs):
        """ Decorated function to updates images in mongo """
        return self.images.update_one(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _images_update_many(self, *args, **kwargs):
        """ Decorated function to updates images in mongo """
        return self.images.update_many(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _images_find(self, *args, **kwargs):
        """ Decorated function to find images in mongo """
        return self.images.find(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _images_find_one(self, *args, **kwargs):
        """ Decorated function to find one image in mongo """
        return self.images.find_one(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _images_insert(self, *args, **kwargs):
        """ Decorated function to insert an image in mongo """
        return self.images.insert_one(*args, **kwargs).inserted_id

    @mongo_reconnect_reattempt
    def _metrics_insert(self, *args, **kwargs):
        """ Decorated function to insert an image in mongo """
        if self.metrics:
            return self.metrics.insert_one(*args, **kwargs)
