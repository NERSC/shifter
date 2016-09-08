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
This module is provides the interface layer for the image manager.  This compliments
the api module which provides the REST interface.  This module does much of the heavy
lifting for the image manager.  It handles all interactions with the Mongo Database
and dispatches work through Celery.
"""

import json
import sys
import os
import logging
from time import time, sleep
from pymongo import MongoClient
import pymongo.errors
from shifter_imagegw.auth import Authentication
from shifter_imagegw.imageworker import dopull, initqueue, doexpire
import bson
import celery

## decorator function to re-attempt any mongo operation that may have failed
## owing to AutoReconnect (e.g., mongod coming back, etc).  This may increase
## the opportunity for race conditions, and should be more closely considered
## for the insert/update functions
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
    It uses a Mongo Database to track state, uses celery to dispatch work,
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
            log_handler.setLevel(logging.DEBUG)
            self.logger.addHandler(log_handler)
        else:
            self.logger = logger

        self.logger.debug('Initializing image manager')
        self.config = config
        if 'Platforms' not in self.config:
            raise NameError('Platforms not defined')
        self.systems = []
        self.tasks = []
        self.expire_requests = dict()
        self.task_image_id = dict()
        # Time before another pull can be attempted
        self.pullupdatetimeout = 300
        if 'PullUpdateTime' in self.config:
            self.pullupdatetimeout = self.config['PullUpdateTimeout']
        # Max amount of time to allow for a pull
        self.pulltimeout = self.pullupdatetimeout*10
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
        if 'MongoDBURI' in self.config:
            client = MongoClient(self.config['MongoDBURI'])
            db_ = self.config['MongoDB']
            self.images = client[db_].images
        else:
            raise NameError('MongoDBURI not defined')
        initqueue(config)
        # Initialize data structures

    def check_session(self, session, system=None):
        """Check if this is a valid session
        session is a session handle
        """
        if 'magic' not in session:
            self.logger.warn("request recieved with no magic")
            return False
        elif session['magic'] is not self.magic:
            self.logger.warn("request received with bad magic %s", session['magic'])
            return False
        if system is not None and session['system'] != system:
            self.logger.warn("request received with a bad system %s!=%s", session['system'], system)
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

    def _checkread(self, user, imageid):
        """
        Checks if the user has read permissions to the image. (Not Implemented)
        """
        self.logger.warn("unimplemented checkread called for %s for user %s"%(imageid, user))
        return True

    def _resetexpire(self, ident):
        """Reset the expire time.  (Not fully implemented)."""
        # Change expire time for image
        ## TODO shore up expire-time parsing
        (days, hours, minutes, secs) = self.config['ImageExpirationTimeout'].split(':')
        expire = time() + int(secs) + 60 * (int(minutes) + 60 * (int(hours) + 24 * int(days)))
        self._images_update({'_id': ident}, {'$set': {'expiration': expire}})
        return expire

    def new_session(self, auth_string, system):
        """
        Creates a session context that can be used for multiple transactions.
        auth is an auth string that will be passed to the authenication layer.
        Returns a context that can be used for subsequent operations.
        """
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
            self._resetexpire(rec['_id'])
        # TODO: verify access
        return rec


    def imglist(self, session, system):
        """
        list images for a system.
        Image is dictionary with system defined.
        """
        if not self.check_session(session, system):
            raise OSError("Invalid Session")
        query = {'status': 'READY', 'system': system}
        self.update_states()
        records = self._images_find(query)
        resp = []
        for record in records:
            resp.append(record)
        # verify access
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
        Creates a new image in mongo.  If the pull already exist it removes it first.
        """
        # Clean out any existing records
        for rec in self._images_find(image):
            if rec['status'] == 'READY':
                continue
            else:
                self._images_remove({'_id': rec['_id']})
        newimage = {
            'format': 'invalid',#<ext4|squashfs|vfs>
            'arch': 'amd64', #<amd64|...>
            'os': 'linux', #<linux|...>
            'location': '', #<urlencoded location, interpretation dependent on remotetype>
            'remotetype': 'dockerv2', #<file|dockerv2|amazonec2>
            'ostcount': '0', #<integer, number of OSTs to per-copy of the image>
            'replication': '1', #<integer, number of copies to deploy>
            'userAcl': [],
            'tag': [],
            'status': 'INIT',
            'groupAcl': []
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
            'system':image['system'],
            'itype':image['itype'],
            'pulltag':image['tag']
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
            'tag': {'$in':[image['tag']]}
        }
        rec = self._images_find_one(query)
        for record in self._images_find(request):
            status = record['status']
            if status == 'READY' or status == 'SUCCESS':
                continue
            rec = record
            break


        if self._pullable(rec):
            rec = self.new_pull_record(request)
            ident = rec['_id']
            self.logger.debug("Setting state")
            self.update_mongo_state(ident, 'ENQUEUED')
            request['tag'] = request['pulltag']
            self.logger.debug("Calling do pull with queue=%s", request['system'])
            pullreq = dopull.apply_async([request], queue=request['system'], \
                    kwargs={'testmode':testmode})

            memo = "pull request queued s=%s t=%s" \
                    % (request['system'], request['tag'])
            self.logger.info(memo)

            self.update_mongo(ident, {'last_pull': time()})
            self.task_image_id[pullreq] = ident
            self.tasks.append(pullreq)

        return rec

    def update_mongo_state(self, ident, state, info=None):
        """
        Helper function to set the mongo state for an image with _id==ident to state=state.
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
        if rec is not None and 'tag' in rec and not isinstance(rec['tag'], (list)):
            memo = 'Fixing tag for non-list %s %s' % (ident, str(rec['tag']))
            self.logger.info(memo)
            curtag = rec['tag']
            self._images_update({'_id': ident}, {'$set':{'tag':[curtag]}})
        self._images_update({'_id': ident}, {'$addToSet': {'tag': tag}})
        return True

    def remove_tag(self, system, tag):
        """
        Helper function to remove a tag to an image.
        """
        self._images_update({'system': system, 'tag': {'$in': [tag]}},
                            {'$pull': {'tag': tag}}, multi=True)
        return True

    def complete_pull(self, ident, response):
        """
        Transition a completed pull request to an available image.
        """

        self.logger.debug("Complete called for %s %s", ident, str(response))
        pullrec = self._images_find_one({'_id': ident})
        if pullrec is None:
            self.logger.warn('Missing pull request (r=%s)', str(response))
            return
        #Check that this image ident doesn't already exist for this system
        rec = self._images_find_one({'id': response['id'], 'system': pullrec['system']})
        tag = pullrec['pulltag']
        if rec is not None:
            # So we already had this image.
            # Let's delete the pull record.
            # TODO: update the pull time of the matching id
            self.update_mongo(rec['_id'], {'last_pull':time()})

            self._images_remove({'_id': ident})
            # However it could be a new tag.  So let's update the tag
            try:
                rec['tag'].index(response['tag'])
            except:
                self.add_tag(rec['_id'], pullrec['system'], tag)
            return True
        else:
            response['last_pull'] = time()
            self.update_mongo(ident, response)
            self.add_tag(ident, pullrec['system'], tag)


    def update_mongo(self, ident, resp):
        """
        Helper function to set the mongo values for an image with _id==ident.
        """
        setline = dict()
        if 'id' in resp:
            setline['id'] = resp['id']
        if 'entrypoint' in resp:
            setline['ENTRY'] = resp['entrypoint']
        if 'env' in resp:
            setline['ENV'] = resp['env']
        if 'workdir' in resp:
            setline['WORKDIR'] = resp['workdir']
        if 'last_pull' in resp:
            setline['last_pull'] = resp['last_pull']

        self._images_update({'_id': ident}, {'$set': setline})

    def get_state(self, ident):
        """
        Lookup the state of the image with _id==ident in Mongo.  Returns the state."""
        self.update_states()
        rec = self._images_find_one({'_id': ident}, {'status': 1})
        if rec is None:
            return None
        elif 'status' not in rec:
            return None
        return rec['status']

    def update_states(self):
        """
        Update the states of all active transactions.
        Cleanup failed transcations after a period
        """
        #logger.debug("Update_states called")
        i = 0
        tasks = self.tasks

        for req in tasks:
            state = 'PENDING'

            if isinstance(req, celery.result.AsyncResult):
                state = req.state
                info = req.info
            elif isinstance(req, bson.objectid.ObjectId):
                self.logger.debug("Non-Async")

            if req in self.expire_requests and state == 'SUCCESS':
                self.expire_requests.pop(req)
                self.tasks.remove(req)
                state = 'EXPIRED'
            elif req in self.expire_requests and state == 'FAILURE':
                self.logger.warn("Expire request failed for %s", req)
                self.expire_requests.pop(req)
                self.tasks.remove(req)
                continue
            elif state == "FAILURE":
                self.logger.warn("Pull failed for %s", req)

            self.update_mongo_state(self.task_image_id[req], state, info)
            if state == "READY" or state == "SUCCESS":
                self.logger.debug("Completing pull request %d", i)
                response = req.get()
                self.complete_pull(self.task_image_id[req], response)
                self.logger.debug('meta=%s', str(response))
                # Now save the response
                self.tasks.remove(req)
            i += 1
        # Look for failed pulls
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
        pulltimeout = time() - self.pullupdatetimeout * 10
        removed = []
        for rec in self._images_find({'status': {'$ne': 'READY'}, 'system': system}):
            self.logger.debug(rec)
            if 'last_pull' not in rec:
                self.logger.warning('Image missing last_pull for pulltag:' + rec['pulltag'])
                continue
            if rec['last_pull'] < pulltimeout:
                removed.append(rec['_id'])
                self._images_remove({'_id': rec['_id']})

        expired = []
        # Look for READY images that haven't been pulled recently
        for rec in self._images_find({'status': 'READY', 'system': system}):
            self.logger.debug(rec)
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
        memo = "Calling do expire with queue=%s id=%s TM=%d" % (rec['system'], ident, testmode)
        self.logger.debug(memo)

        req = doexpire.apply_async([rec], queue=rec['system'])
        self.logger.info("expire request queued s=%s t=%s", rec['system'], ident)
        self.task_image_id[req] = ident
        self.expire_requests[req] = ident
        self.tasks.append(req)


    def expire(self, session, image, testmode=0):
        """Expire an image.  (Not Implemented)"""
        if not self._isadmin(session, image['system']):
            return False
        query = {
            'system': image['system'],
            'itype': image['itype'],
            'tag': {'$in':[image['tag']]}
        }
        rec = self._images_find_one(query)
        if rec is None:
            return None
        ident = rec.pop('_id')
        memo = "Calling do expire with queue=%s id=%s TM=%d" \
                % (image['system'], ident, testmode)
        self.logger.debug(memo)

        req = doexpire.apply_async([rec], queue=image['system'], \
                kwargs={'testmode':testmode})

        memo = "expire request queued s=%s t=%s" \
                % (image['system'], image['tag'])
        self.logger.info(memo)

        self.task_image_id[req] = ident
        self.expire_requests[req] = ident
        self.tasks.append(req)

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
