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
DB Interface
"""

import os
import logging
from time import time, sleep
from pymongo import MongoClient
import pymongo.errors
from shifter_imagegw.config import Config


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


class DB(object):
    """
    This class handles most of the backend work for the image gateway.
    It uses a Mongo Database to track state, uses threads to dispatch work,
    and has public functions to lookup, pull and expire images.
    """

    def __init__(self, config: Config):
        """
        Create an instance of the db interface.
        """
        client = MongoClient(config.MongoDBURI)
        db = config.MongoDB
        self.images = client[db].images
        self.metrics = None
        self.pullupdatetimeout = config.PullUpdateTimeout
        if config.Metrics:
            self.metrics = client[db].metrics

    def add_tag(self, ident: str, system: str, tag):
        """
        Helper function to add a tag to an image.
        ident is the mongo id (not image id)
        """
        # Remove the tag first
        self.remove_tag(system, tag)
        # see if tag isn't a list
        rec = self.images_find_one({'_id': ident})
        if rec is not None and 'tag' in rec and \
                not isinstance(rec['tag'], (list)):
            memo = f'Fixing tag for non-list {ident} {str(rec["tag"])}'
            logging.info(memo)
            curtag = rec['tag']
            self.images_update({'_id': ident}, {'$set': {'tag': [curtag]}})
        self.images_update({'_id': ident}, {'$addToSet': {'tag': tag}})
        return True

    def remove_tag(self, system: str, tag: str):
        """
        Helper function to remove a tag to an image.
        """
        self.images_update_many({'system': system, 'tag': {'$in': [tag]}},
                                {'$pull': {'tag': tag}})
        return True

    def update_image(self, ident: str, resp: dict):
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

        self.images_update({'_id': ident}, {'$set': setline})

    def update_image_state(self, ident: str, state: str,
                           info: dict | None = None):
        """
        Helper function to set the mongo state for an image with _id==ident
        to state=state.
        """
        if state == 'SUCCESS':
            state = 'READY'
        set_list = {'status': state, 'status_message': ''}
        if info and isinstance(info, dict):
            if 'heartbeat' in info:
                set_list['last_heartbeat'] = info['heartbeat']
            if 'message' in info:
                set_list['status_message'] = info['message']
        self.images_update({'_id': ident}, {'$set': set_list})

    def get_state(self, ident: str):
        """
        Lookup the state of the image with _id==ident in Mongo.
        Returns the state.
        """
        self.update_states()
        rec = self.images_find_one({'_id': ident}, {'status': 1})
        if rec is None:
            return None
        elif 'status' not in rec:
            return None
        return rec['status']

    def update_states(self):
        """
        Cleanup failed transcations after a period
        """
        for rec in self.images_find({'status': 'FAILURE'}):
            nextpull = self.pullupdatetimeout + rec['last_pull']
            # It it has been a while then let's clean up
            if time() > nextpull:
                self.images_remove({'_id': rec['_id']})

    def get_metrics(self, limit: int):
        """
        Return the last <limit> lookup records.
        """
        count = self.metrics.count_documents({})
        skip = count - limit
        if skip < 0:
            skip = 0
        recs = []
        for r in self.metrics.find().skip(skip):
            r.pop('_id', None)
            recs.append(r)
        return recs

    @mongo_reconnect_reattempt
    def images_remove(self, *args, **kwargs):
        """ Decorated function to remove images from mongo """
        return self.images.delete_one(*args, **kwargs)

    @mongo_reconnect_reattempt
    def images_remove_many(self, *args, **kwargs):
        """ Decorated function to remove images from mongo """
        return self.images.delete_many(*args, **kwargs)

    @mongo_reconnect_reattempt
    def images_update(self, *args, **kwargs):
        """ Decorated function to updates images in mongo """
        return self.images.update_one(*args, **kwargs)

    @mongo_reconnect_reattempt
    def images_update_many(self, *args, **kwargs):
        """ Decorated function to updates images in mongo """
        return self.images.update_many(*args, **kwargs)

    @mongo_reconnect_reattempt
    def images_find(self, *args, **kwargs):
        """ Decorated function to find images in mongo """
        return self.images.find(*args, **kwargs)

    @mongo_reconnect_reattempt
    def images_find_one(self, *args, **kwargs):
        """ Decorated function to find one image in mongo """
        return self.images.find_one(*args, **kwargs)

    @mongo_reconnect_reattempt
    def images_insert(self, *args, **kwargs):
        """ Decorated function to insert an image in mongo """
        return self.images.insert_one(*args, **kwargs).inserted_id

    @mongo_reconnect_reattempt
    def metrics_insert(self, *args, **kwargs):
        """ Decorated function to insert an image in mongo """
        if self.metrics:
            return self.metrics.insert_one(*args, **kwargs)
