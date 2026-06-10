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
interactions with the DB interface and dispatches work through a thread pool.
"""

import logging
from time import time
from shifter_imagegw.imageworker import WorkerThreads
from shifter_imagegw.imageworker import PullRequest
from shifter_imagegw.imageworker import ImportRequest
from shifter_imagegw.imageworker import ExpireRequest
from shifter_imagegw.db import DB
from shifter_imagegw.models import Session, Request
from shifter_imagegw.config import Config
import grp
from multiprocessing import Process
import atexit
from cachetools import cached, TTLCache, LFUCache


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
        self.pullupdatetimeout = config.PullUpdateTimeout
        # Max amount of time to allow for a pull
        self.pulltimeout = self.pullupdatetimeout

        # Connect to database
        threads = config.WorkerThreads
        self.workers = WorkerThreads(config, threads=threads)
        self.status_queue = self.workers.get_updater_queue()
        self.status_proc = Process(target=self.status_thread,
                                   kwargs={"config": config},
                                   name='StatusThread')
        self.status_proc.start()
        atexit.register(self.shutdown)
        self.db = DB(config)
        # Cleanup any pending requests
        self.db.remove_pending_images()
        self.platforms = config.Platforms
        self.systems = config.Platforms.keys()
        self.config = config

    def shutdown(self):
        logging.info("Shutdown called")
        self.status_queue.put('stop')
        self.status_proc.terminate()

    def status_thread(self, config: Config | None = None):
        """
        This listens for update messages from a queue.
        """
        self.db = DB(config)
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
                self.db.update_image_state(ident, state, meta)
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

    def _compare_list(self, a: list[int] | None, b: list[int] | None):
        """"
        look at the key element of two objects
        and compare the list of ids.

        return True if everything matches
        return False if anything is different
        """

        # If the key isn't in the objects or
        # something else fails, then it must
        # have changed.
        if a == b:
            return True

        if a is None or b is None:
            return False
        if len(a) != len(b):
            return False
        for item in a:
            if item not in b:
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
            self.db.metrics_insert(r)
        except Exception:
            logging.warning('Failed to log lookup.')

    def get_metrics(self, session: Session, limit: int):
        """
        Return the last <limit> lookup records.
        """
        if not session.admin:
            return []
        if not self.db.metrics:
            return []
        return self.db.get_metrics(limit)

    @cached(cache=TTLCache(maxsize=100, ttl=60), info=True)
    def lookup(self,
               session: Session,
               itype: str,
               tag: str
               ):
        """
        Lookup an image.
        Image is dictionary with system,itype and tag defined.
        """
        self.db.update_states()
        rec = self.db.find_image_by(status='READY',
                                    system=session.system,
                                    image_type=itype,
                                    tag=tag)
        if rec:
            if self._checkread(session, rec) is False:
                return None
            self.db.reset_expire(rec['_id'], self.config.expire_secs)

        if rec and self.db.metrics:
            self._add_metrics(session,
                              session.system,
                              itype,
                              tag,
                              rec['id'])
        return rec

    def imglist(self, session: Session):
        """
        list images for a system.
        Image is dictionary with system defined.
        """
        self.db.update_states()
        records = self.db.find_many_images_by(status='READY',
                                              system=session.system)
        resp = []
        for record in records:
            if self._checkread(session, record):
                resp.append(record)
        # verify access
        return resp

    def show_queue(self, session: Session):
        """
        list queue for a system.
        Image is dictionary with system defined.
        """
        self.db.update_states()
        records = self.db.find_many_images_by(status='NOT_READY',
                                              system=session.system)
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
        for rec in self.db.find_many_images_by(status='NOT_READY',
                                               system=image['system'],
                                               image_type=image['itype'],
                                               pulltag=image['pulltag']):
            self.db.remove_image_by_id(rec['_id'])

        # TODO: replace this
        newimage = {
            'format': 'invalid',  # <ext4|squashfs|vfs>
            'userACL': [],
            'groupACL': [],
            'private': None,
            'tag': [],
            'status': 'INIT',
            'last_pull': time()
        }
        newimage['format'] = self.config.DefaultImageFormat
        for param in image:
            if param == 'tag':
                continue
            newimage[param] = image[param]
        self.db.images_insert(newimage)
        return newimage

    def pull(self, session: Session, req: Request):
        """
        pull the image
        Takes an auth token, a request object
        """
        # If a pull request exist for this tag
        #  check to see if it is expired or a failure, if so remove it
        # otherwise
        #  return the record
        rec = None
        # find any pull record
        self.db.update_states()
        # let's lookup the active image
        rec = self.db.find_image_by(status="READY",
                                    system=session.system,
                                    image_type=req.itype,
                                    tag=req.tag)
        recs = self.db.find_many_images_by(system=req.system,
                                           image_type=req.itype,
                                           pulltag=req.tag)
        for record in recs:
            if record['status'] in ['READY', 'SUCCESS']:
                continue
            rec = record
            break

        # TODO: Clean this up
        inflight = False
        recent = False
        if rec is not None and rec['status'] != 'READY':
            inflight = True
        elif rec is not None:
            # if an image has been pulled in the last 60 seconds
            # let's consider that "recent"
            if (time() - rec['last_pull']) < 60:
                recent = True
        # TODO: refactor the compare to not require the dicitonary
        if rec and self._compare_list(req.userACL, rec.get('userACL')) and \
                self._compare_list(req.groupACL, rec.get('groupACL')):
            acl_changed = False
        else:
            logging.debug("ACL change detected.")
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
            request = req.pull_record(session)
            rec = self.new_pull_record(request)
            ident = rec['_id']
            logging.debug("PENDING Request")
            self.db.update_image_state(ident, 'PENDING')
            logging.debug("Calling do pull with queue="
                          f"{request['system']}")
            pr = PullRequest(self.config,
                             req.tag,
                             ident,
                             session,
                             useracl=request['userACL'],
                             groupacl=request['groupACL'])
            self.workers.submit(pr)

            memo = "pull request queued s={req.system} tag={req.tag}"
            logging.info(memo)

        return rec

    def mngrimport(self, session: Session, req: Request):
        """
        import the image directly from a file
        Only for allowed users
        Takes an auth token, a request object
        """
        meta = {}
        logging.debug(f'mngrmport called for file {req.filepath}')
        # Skip checks about previous requests for now
        # Future work could check the fasthash and
        # not import if they're the same
        rec = self.db.find_image_by(system=req.system,
                                    image_type=req.itype,
                                    pulltag=req.tag)
        if not self._pullable(rec):
            return rec

        # We could hit a key error or some other edge case
        # so just do our best and update if there are problems

        logging.debug("Creating New Import Record")
        # new_pull_record works for import too
        request = {
            'system': req.system,
            'itype': req.itype,
            'pulltag': req.tag,
            'filepath': req.filepath,
            'format': req.format,
            'meta': meta,
            'last_pull': time()
        }
        rec = self.new_pull_record(request)
        ident = rec['_id']
        logging.debug(f"PENDING Request, ident {ident}")
        self.db.update_image_state(ident, 'PENDING')
        request['tag'] = request['pulltag']
        request['session'] = session
        logging.debug("Calling wrkimport with queue="
                      "{request['system']}")
        ir = ImportRequest(self.config,
                           session.system,
                           req.tag,
                           ident,
                           session,
                           req.filepath)
        self.workers.submit(ir)

        memo = "import request queued " \
               f"s={req.system} tag={req.tag}"
        logging.info(memo)
        return rec

    def update_acls(self, ident: str, response: dict):
        logging.debug(f"Update ACLs called for {ident} {str(response)}")
        pullrec = self.db.find_image_by(id=ident)
        if pullrec is None:
            logging.error('ERROR: Missing pull request resp=',
                          f'{str(response)}')
            return
        # Check that this image ident doesn't already exist for this system
        rec = self.db.find_image_by(image_id=response['id'], status='READY',
                                    system=pullrec['system'])
        if rec is None:
            # This means the image already existed, but we didn't have a
            # record of it.  That seems odd (it happens in tests).  Let's
            # note it and power on through.
            msg = "WARNING: No image record found for an ACL update"
            logging.warning(msg)
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.db.update_image(ident, response)
            self.db.add_tag(ident, pullrec['system'], pullrec['pulltag'])
        else:
            self.db.add_tag(rec['_id'], pullrec['system'], pullrec['pulltag'])
            updates = {
                'userACL': response['userACL'],
                'groupACL': response['groupACL'],
                'private': response['private'],
                'last_pull': time()
            }
            logging.debug("Doing ACLs update")
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.db.update_image(rec['_id'], updates)
            self.db.remove_image_by_id(ident)

    def complete_pull(self, ident: str, response: dict):
        """
        Transition a completed pull request to an available image.
        """

        logging.debug(f"Complete called for {ident} {str(response)}")
        pullrec = self.db.find_image_by(id=ident)
        if pullrec is None:
            logging.warning(f'Missing pull request resp={str(response)}')
            return
        # Check that this image ident doesn't already exist for this system
        rec = self.db.find_image_by(image_id=response['id'],
                                    system=pullrec['system'],
                                    status='READY')
        tag = pullrec['pulltag']
        if rec is not None:
            # So we already had this image.
            # Let's delete the pull record.
            # TODO: update the pull time of the matching id
            logging.warning('Duplicate image')
            update_rec = {
                'last_pull': time()
            }
            self.db.update_image(rec['_id'], update_rec)

            self.db.remove_image_by_id(ident)
            # However it could be a new tag.  So let's update the tag
            try:
                rec['tag'].index(response['tag'])
            except ValueError:
                self.db.add_tag(rec['_id'], pullrec['system'], tag)
            return True
        else:
            response['last_pull'] = time()
            response['status'] = 'READY'
            self.db.update_image(ident, response)
            self.db.add_tag(ident, pullrec['system'], tag)

    def autoexpire(self, session: Session):
        """Auto expire images and do cleanup"""
        # While this should be safe, let's restrict this to admins
        if not session.admin:
            return False
        # Cleanup - Lookup for things stuck in non-READY state
        self.db.update_states()
        removed = []
        for rec in self.db.find_many_images_by(status='NOT_READY',
                                               system=session.system):
            if 'last_pull' not in rec:
                logging.warning('Image missing last_pull for pulltag:' +
                                rec['pulltag'])
                continue
            if time() > rec['last_pull'] + self.pulltimeout:
                removed.append(rec['_id'])
                self.db.remove_image_by_id(rec['_id'])

        expired = []
        # Look for READY images that haven't been pulled recently
        for rec in self.db.find_many_images_by(status='READY',
                                               system=session.system):
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
        if not session.admin:
            return False
        rec = self.db.find_image_by(system=image['system'],
                                    image_type=image['itype'],
                                    tag=image['tag'])
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
