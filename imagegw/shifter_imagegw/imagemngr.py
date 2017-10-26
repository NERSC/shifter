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
Imagae Manager for the Shifter Gateway.

This module is provides the interface layer for the image manager.  This
compliments the api module which provides the REST interface.  This module
does much of the heavy lifting for the image manager.  It handles all
interactions with the Mongo Database and dispatches work through a thread pool.
"""

import json
import sys
import os
import logging
from time import time, sleep
from pymongo import MongoClient
import pymongo.errors
from shifter_imagegw.auth import Authentication
from shifter_imagegw.imageworker import WorkerThreads
from multiprocessing.process import Process


# decorator function to re-attempt any mongo operation that may have failed
# owing to AutoReconnect (e.g., mongod coming back, etc).  This may increase
# the opportunity for race conditions, and should be more closely considered
# for the insert/update functions
def mongo_reconnect_reattempt(call):
    """Automatically re-attempt potentially failed mongo operations"""
    def _mongo_reconnect_safe(self, *args, **kwargs):
        for _ in xrange(2):
            try:
                return call(self, *args, **kwargs)
            except pymongo.errors.AutoReconnect:
                self.logger.warn("Error: mongo reconnect attmempt")
                sleep(2)
        self.logger.warn("Error: Failed to deal with mongo auto-reconnect!")
        raise OSError('Reconnect to mongo failed')
    return _mongo_reconnect_safe


class ImageMngr(object):
    """
    This class handles most of the backend work for the image gateway.
    It uses a Mongo Database to track state, uses threads to dispatch work,
    and has public functions to lookup, pull and expire images.
    """

    def __init__(self, config, logger=None, logname='imagemngr'):
        """
        Create an instance of the image manager.
        """
        if logger is None:
            self.logger = logging.getLogger(logname)
            log_handler = logging.StreamHandler()
            logfmt = '%(asctime)s [%(name)s] %(levelname)s : %(message)s'
            log_handler.setFormatter(logging.Formatter(logfmt))
            log_handler.setLevel(logging.INFO)
            self.logger.addHandler(log_handler)
        else:
            self.logger = logger

        self.logger.debug('Initializing image manager')
        self.config = config
        if 'Platforms' not in self.config:
            raise NameError('Platforms not defined')
        self.systems = []
        # Time before another pull can be attempted
        self.pullupdatetimeout = 300
        if 'PullUpdateTime' in self.config:
            self.pullupdatetimeout = self.config['PullUpdateTimeout']
        # Max amount of time to allow for a pull
        self.pulltimeout = self.pullupdatetimeout
        # This is not intended to provide security, but just
        # provide a basic check that a session object is correct
        self.magic = 'imagemngrmagic'
        if 'Authentication' not in self.config:
            self.config['Authentication'] = "munge"
        self.auth = Authentication(self.config)
        self.platforms = self.config['Platforms']

        for system in self.config['Platforms']:
            self.systems.append(system)
        # Connect to database
        if 'MongoDBURI' not in self.config:
            raise NameError('MongoDBURI not defined')
        self.workers = WorkerThreads()
        self.status_queue = self.workers.get_updater_queue()
        self.status_proc = Process(target=self.status_thread,
                                   name='StatusThread')
        self.status_proc.start()
        client = MongoClient(self.config['MongoDBURI'])
        db_ = self.config['MongoDB']
        self.images = client[db_].images
        self.metrics = None
        if 'Metrics' in self.config and self.config['Metrics'] is True:
            self.metrics = client[db_].metrics
        # Initialize data structures

    def shutdown(self):
        self.status_queue.put('stop')

    def status_thread(self):
        """
        This listens for update messages from a queue.
        """
        client = MongoClient(self.config['MongoDBURI'])
        db_ = self.config['MongoDB']
        self.images = client[db_].images
        while True:
            message = self.status_queue.get()
            if message == 'stop':
                self.logger.info("Shutting down Status Thread")
                break
            ident = message['id']
            state = message['state']
            meta = message['meta']
            # TODO: Handle a failed expire
            if state == "FAILURE":
                self.logger.warn("Operation failed for %s", ident)

            # print "Status: %s" % (state)
            # A response message
            if state != 'READY':
                self.update_mongo_state(ident, state, meta)
                continue
            if 'response' in meta and meta['response']:
                response = meta['response']
                self.logger.debug(response)
                if 'meta_only' in response:
                    self.logger.debug('Updating ACLs')
                    self.update_acls(ident, response)
                else:
                    self.complete_pull(ident, response)
                self.logger.debug('meta=%s', str(response))

    def check_session(self, session, system=None):
        """Check if this is a valid session
        session is a session handle
        """
        if 'magic' not in session:
            self.logger.warn("request recieved with no magic")
            return False
        elif session['magic'] is not self.magic:
            self.logger.warn("request received with bad magic %s",
                             session['magic'])
            return False
        if system is not None and session['system'] != system:
            self.logger.warn("request received with a bad system %s!=%s",
                             session['system'], system)
            return False
        return True

    def _isadmin(self, session, system=None):
        """
        Check if this is an admin user.
        Returns true if is an admin or false if not.
        """
        if 'admins' not in self.platforms[system]:
            return False
        admins = self.platforms[system]['admins']
        user = session['user']
        if user in admins:
            self.logger.info('user %s is an admin', user)
            return True
        return False

    def _isasystem(self, system):
        """Check if system is a valid platform."""
        return bool(system in self.systems)

    def _checkread(self, session, rec):
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
        uid = session['uid']
        gid = session['gid']
        self.logger.debug('uid=%s iUACL=%s' % (uid, str(iUACL)))
        self.logger.debug('sessions = ' + str(session))
        if iUACL is not None and uid in iUACL:
            return True
        if iGACL is not None and gid in iGACL:
            return True
        return False

    def _resetexpire(self, ident):
        """Reset the expire time.  (Not fully implemented)."""
        # Change expire time for image
        # TODO shore up expire-time parsing
        expire_timeout = self.config['ImageExpirationTimeout']
        (days, hours, minutes, secs) = expire_timeout.split(':')
        expire = time() + int(secs) + 60 * (int(minutes) +
                                            60 * (int(hours) + 24 * int(days)))
        self._images_update({'_id': ident}, {'$set': {'expiration': expire}})
        return expire

    def _make_acl(self, acllist, id):
        if id not in acllist:
            acllist.append(id)
        return acllist

    def _compare_list(self, a, b, key):
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
        except:
            return True
        aitems = a[key]
        bitems = b[key]
        if len(aitems) != len(bitems):
            return False
        for item in aitems:
            if item not in bitems:
                return False
        return True

    def _add_metrics(self, session, request, record):
        """
        Add a row to mongo for this lookup request.
        """
        try:
            r = {
                'user': session['user'],
                'uid': session['uid'],
                'system': request['system'],
                'type': request['itype'],
                'tag': request['tag'],
                'id': record['id'],
                'time': time()
            }
            self._metrics_insert(r)
        except:
            self.logger.warn('Failed to log lookup.')

    def get_metrics(self, session, system, limit):
        """
        Return the last <limit> lookup records.
        """
        recs = []
        if not self._isadmin(session, system):
            return recs
        if self.metrics is None:
            return recs
        count = self.metrics.count()
        skip = count - limit
        if skip < 0:
            skip = 0
        for r in self.metrics.find().skip(skip):
            r.pop('_id', None)
            recs.append(r)
        return recs

    def new_session(self, auth_string, system):
        """
        Creates a session context that can be used for multiple transactions.
        auth is an auth string that will be passed to the authenication layer.
        Returns a context that can be used for subsequent operations.
        """
        if auth_string is None:
            return {'magic': self.magic, 'system': system}
        arec = self.auth.authenticate(auth_string, system)
        if arec is None and isinstance(arec, dict):
            raise OSError("Authenication returned None")
        else:
            if 'user' not in arec:
                raise OSError("Authentication returned invalid response")
            session = arec
            session['magic'] = self.magic
            session['system'] = system
            return session

    def lookup(self, session, image):
        """
        Lookup an image.
        Image is dictionary with system,itype and tag defined.
        """
        if not self.check_session(session, image['system']):
            raise OSError("Invalid Session")
        query = {
            'status': 'READY',
            'system': image['system'],
            'itype': image['itype'],
            'tag': {'$in': [image['tag']]}
        }
        self.update_states()
        rec = self._images_find_one(query)
        if rec is not None:
            if self._checkread(session, rec) is False:
                return None
            self._resetexpire(rec['_id'])

        if self.metrics is not None:
            self._add_metrics(session, image, rec)
        return rec

    def imglist(self, session, system):
        """
        list images for a system.
        Image is dictionary with system defined.
        """
        if not self.check_session(session, system):
            raise OSError("Invalid Session")
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

    def show_queue(self, session, system):
        """
        list queue for a system.
        Image is dictionary with system defined.
        """
        if not self.check_session(session, system):
            raise OSError("Invalid Session")
        query = {'status': {'$ne': 'READY'}, 'system': system}
        self.update_states()
        records = self._images_find(query)
        resp = []
        for record in records:
            resp.append({'status': record['status'],
                        'image': record['pulltag']})
        return resp

    def _isready(self, image):
        """Helper function to determine if an image is READY."""
        query = {
            'status': 'READY',
            'system': image['system'],
            'itype': image['itype'],
            'tag': {'$in': [image['tag']]}
        }
        rec = self._images_find_one(query)
        if rec is not None:
            return True
        return False

    def _pullable(self, rec):
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

    def new_pull_record(self, image):
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
            'arch': 'amd64',  # <amd64|...>
            'os': 'linux',  # <linux|...>
            'location': '',  # urlencoded location
            'remotetype': 'dockerv2',  # <file|dockerv2|amazonec2>
            'ostcount': '0',  # integer, number of OSTs (future)
            'replication': '1',  # integer, number of copies to deploy
            'userACL': [],
            'groupACL': [],
            'private': None,
            'tag': [],
            'status': 'INIT'
        }
        if 'DefaultImageFormat' in self.config:
            newimage['format'] = self.config['DefaultImageFormat']
        for param in image:
            if param is 'tag':
                continue
            newimage[param] = image[param]
        self._images_insert(newimage)
        return newimage

    def pull(self, session, image, testmode=0):
        """
        pull the image
        Takes an auth token, a request object
        Optional: testmode={0,1,2} See below...
        """
        request = {
            'system': image['system'],
            'itype': image['itype'],
            'pulltag': image['tag']
        }
        self.logger.debug('Pull called Test Mode=%d', testmode)
        if not self.check_session(session, request['system']):
            self.logger.warn('Invalid session on system %s', request['system'])
            raise OSError("Invalid Session")
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
                                                session['uid'])
        if 'groupACL' in image and image['groupACL'] != []:
            request['groupACL'] = self._make_acl(image['groupACL'],
                                                 session['gid'])
        if self._compare_list(request, rec, 'userACL') and \
                self._compare_list(request, rec, 'groupACL'):
            acl_changed = False
        else:
            self.logger.debug("No ACL change detected.")
            acl_changed = True

        # We could hit a key error or some other edge case
        # so just do our best and update if there are problems
        update = False
        if not recent and not inflight and acl_changed:
            self.logger.debug("ACL change detected.")
            update = True

        if self._pullable(rec):
            self.logger.debug("Pullable image")
            update = True

        if update:
            self.logger.debug("Creating New Pull Record")
            rec = self.new_pull_record(request)
            ident = rec['_id']
            self.logger.debug("ENQUEUEING Request")
            self.update_mongo_state(ident, 'ENQUEUED')
            request['tag'] = request['pulltag']
            request['session'] = session
            self.logger.debug("Calling do pull with queue=%s",
                              request['system'])
            self.workers.dopull(ident, request, testmode=testmode)

            memo = "pull request queued s=%s t=%s" \
                % (request['system'], request['tag'])
            self.logger.info(memo)

            self.update_mongo(ident, {'last_pull': time()})

        return rec

    def update_mongo_state(self, ident, state, info=None):
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

    def add_tag(self, ident, system, tag):
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
            memo = 'Fixing tag for non-list %s %s' % (ident, str(rec['tag']))
            self.logger.info(memo)
            curtag = rec['tag']
            self._images_update({'_id': ident}, {'$set': {'tag': [curtag]}})
        self._images_update({'_id': ident}, {'$addToSet': {'tag': tag}})
        return True

    def remove_tag(self, system, tag):
        """
        Helper function to remove a tag to an image.
        """
        self._images_update({'system': system, 'tag': {'$in': [tag]}},
                            {'$pull': {'tag': tag}}, multi=True)
        return True

    def update_acls(self, ident, response):
        self.logger.debug("Update ACLs called for %s %s", ident, str(response))
        pullrec = self._images_find_one({'_id': ident})
        if pullrec is None:
            self.logger.error('ERROR: Missing pull request (r=%s)',
                              str(response))
            return
        # Check that this image ident doesn't already exist for this system
        rec = self._images_find_one({'id': response['id'], 'status': 'READY',
                                    'system': pullrec['system']})
        if rec is None:
            # This means the image already existed, but we didn't have a
            # record of it.  That seems odd (it happens in tests).  Let's
            # note it and power on through.
            msg = "WARNING: No image record found for an ACL update"
            self.logger.warn(msg)
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.update_mongo(ident, response)
            self.add_tag(ident, pullrec['system'], pullrec['pulltag'])
        else:
            updates = {
                'userACL': response['userACL'],
                'groupACL': response['groupACL'],
                'private': response['private'],
                'last_pull': time()
            }
            self.logger.debug("Doing ACLs update")
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.update_mongo(rec['_id'], updates)
            self._images_remove({'_id': ident})

    def complete_pull(self, ident, response):
        """
        Transition a completed pull request to an available image.
        """

        self.logger.debug("Complete called for %s %s", ident, str(response))
        pullrec = self._images_find_one({'_id': ident})
        if pullrec is None:
            self.logger.warn('Missing pull request (r=%s)', str(response))
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
            self.logger.warn('Duplicate image')
            update_rec = {
                'last_pull': time()
            }
            self.update_mongo(rec['_id'], update_rec)

            self._images_remove({'_id': ident})
            # However it could be a new tag.  So let's update the tag
            try:
                rec['tag'].index(response['tag'])
            except:
                self.add_tag(rec['_id'], pullrec['system'], tag)
            return True
        else:
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.update_mongo(ident, response)
            self.add_tag(ident, pullrec['system'], tag)

    def update_mongo(self, ident, resp):
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
        if 'private' in resp and resp['private'] is False:
            resp['userACL'] = []
            resp['groupACL'] = []

        for key in mappings.keys():
            if key in resp:
                setline[mappings[key]] = resp[key]

        self._images_update({'_id': ident}, {'$set': setline})

    def get_state(self, ident):
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

    def autoexpire(self, session, system, testmode=0):
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
                self.logger.warning('Image missing last_pull for pulltag:' +
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
                self.logger.debug("expiring %s", rec['id'])
                ident = rec.pop('_id')
                self.expire_id(rec, ident)
                if 'id' in rec:
                    expired.append(rec['id'])
                else:
                    expired.append('unknown')
            self.logger.debug(rec['expiration'] > time())
        return expired

    def expire_id(self, rec, ident, testmode=0):
        """ Helper function to expire by id """
        memo = "Calling do expire id=%s TM=%d" \
            % (ident, testmode)
        self.logger.debug(memo)

        self.workers.doexpire(ident, rec)
        self.logger.info("expire request queued s=%s t=%s",
                         rec['system'], ident)

    def expire(self, session, image, testmode=0):
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
        memo = "Calling do expire with queue=%s id=%s TM=%d" \
            % (image['system'], ident, testmode)
        self.logger.debug(memo)
        self.workers.doexpire(ident, rec)

        memo = "expire request queued s=%s t=%s" \
            % (image['system'], image['tag'])
        self.logger.info(memo)

        return True

    @mongo_reconnect_reattempt
    def _images_remove(self, *args, **kwargs):
        """ Decorated function to remove images from mongo """
        return self.images.remove(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _images_update(self, *args, **kwargs):
        """ Decorated function to updates images in mongo """
        return self.images.update(*args, **kwargs)

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
        return self.images.insert(*args, **kwargs)

    @mongo_reconnect_reattempt
    def _metrics_insert(self, *args, **kwargs):
        """ Decorated function to insert an image in mongo """
        if self.metrics is not None:
            return self.metrics.insert(*args, **kwargs)


def usage():
    """Print usage"""
    print "Usage: imagemngr <lookup|pull|expire>"
    sys.exit(0)


def main():
    """ Main function. This is mainly for testing purposes. """
    configfile = 'test.json'
    if 'CONFIG' in os.environ:
        configfile = os.environ['CONFIG']
    with open(configfile) as handle:
        config = json.load(handle)
    mgr = ImageMngr(config)
    sys.argv.pop(0)
    if len(sys.argv) < 1:
        usage()
        sys.exit(0)
    command = sys.argv.pop(0)
    if command == 'lookup':
        if len(sys.argv) < 3:
            usage()
    elif command == 'list':
        if len(sys.argv) < 1:
            usage()
    elif command == 'pull':
        if len(sys.argv) < 3:
            usage()
        req = dict()
        (req['system'], req['itype'], req['tag']) = sys.argv[0:3]
        mgr.pull('good', req)
    else:
        print "Unknown command %s" % (command)
        usage()


if __name__ == '__main__':
    main()
