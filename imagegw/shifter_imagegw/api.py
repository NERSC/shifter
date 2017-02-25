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
This module provides the REST API for the image gateway.
"""

import json
import os
import sys
import logging
import shifter_imagegw
from shifter_imagegw.imagemngr import ImageMngr
from flask import Flask, request, jsonify


app = Flask(__name__)
config = {}
AUTH_HEADER = 'authentication'


if 'GWCONFIG' in os.environ:
    CONFIG_FILE = os.environ['GWCONFIG']
else:
    CONFIG_FILE = '%s/imagemanager.json' % (shifter_imagegw.CONFIG_PATH)

app.logger.setLevel(logging.INFO)
app.debug_log_format = '%(asctime)s [%(name)s] %(levelname)s : %(message)s'
app.logger.debug('Initializing image manager')

app.logger.info("initializing with %s" % (CONFIG_FILE))
with open(CONFIG_FILE) as config_file:
    config = json.load(config_file)
    if 'LogLevel' in config:
        LOG_STRING = config['LogLevel'].lower()
        if LOG_STRING == 'debug':
            app.logger.setLevel(logging.DEBUG)
        elif LOG_STRING == 'info':
            app.logger.setLevel(logging.INFO)
        elif LOG_STRING == 'warn':
            app.logger.setLevel(logging.WARN)
        elif LOG_STRING == 'error':
            app.logger.setLevel(logging.ERROR)
        elif LOG_STRING == 'critical':
            app.logger.setLevel(logging.CRITICAL)
        else:
            app.logger.critical('Unrecongnized Log Level specified')
mgr = ImageMngr(config, logger=app.logger)


# For RESTful Service
@app.errorhandler(404)
def not_found(error=None):
    """ Standard error function to return a 404. """
    app.logger.warning("404 return")
    message = {
        'status': 404,
        'error': str(error),
        'message': 'Not Found: ' + request.url,
    }
    resp = jsonify(message)
    resp.status_code = 404
    return resp


@app.route('/')
def apihelp():
    """ API helper return """
    return "{lookup,pull,expire,list}"


def create_response(rec):
    """ Helper function to create a formated JSON response. """
    resp = {}
    fields = (
        'id', 'system', 'itype', 'tag', 'status', 'userACL', 'groupACL',
        'ENV', 'ENTRY', 'WORKDIR', 'last_pull', 'status_message',
    )
    for field in fields:
        try:
            resp[field] = rec[field]
        except KeyError:
            resp[field] = 'MISSING'
    return resp


# List images
# This will list the images for a system
@app.route('/api/list/<system>/', methods=["GET"])
def imglist(system):
    """ List images for a specific system. """
    auth = request.headers.get(AUTH_HEADER)
    app.logger.debug("list system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        records = mgr.imglist(session, system)
        if records is None:
            return not_found('image not found')
    except OSError:
        app.logger.warning('Bad session or system')
        return not_found('Bad session or system')
    except:
        app.logger.exception('Unknown Exception in List')
        return not_found('%s' % (sys.exc_value))
    images = []
    for rec in records:
        images.append(create_response(rec))
    resp = {'list': images}
    return jsonify(resp)


# Lookup image
# This will lookup the status of the requested image.
@app.route('/api/lookup/<system>/<imgtype>/<path:tag>/', methods=["GET"])
def lookup(system, imgtype, tag):
    """ Lookup an image for a system and return its record """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    memo = 'lookup system=%s imgtype=%s tag=%s auth=%s' \
           % (system, imgtype, tag, auth)
    app.logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    try:
        session = mgr.new_session(auth, system)
        rec = mgr.lookup(session, i)
        if rec is None:
            app.logger.debug("Image lookup failed.")
            return not_found('image not found')
    except:
        app.logger.exception('Exception in lookup')
        return not_found('%s %s' % (sys.exc_type, sys.exc_value))
    return jsonify(create_response(rec))


# Pull image
# This will pull the requested image.
@app.route('/api/pull/<system>/<imgtype>/<path:tag>/', methods=["POST"])
def pull(system, imgtype, tag):
    """ Pull a specific image and tag for a systems. """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    data = {}
    try:
        rqd = request.get_data()
        if rqd is not "" and rqd is not None:
            data = json.loads(rqd)
    except:
        app.logger.warn("Unable to parse pull data '%s'" %
                        (request.get_data()))
        pass

    memo = "pull system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    app.logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    if 'allowed_uids' in data:
        # Convert to integers
        i['userACL'] = map(lambda x: int(x),
                           data['allowed_uids'].split(','))
    if 'allowed_gids' in data:
        # Convert to integers
        i['groupACL'] = map(lambda x: int(x),
                            data['allowed_gids'].split(','))
    try:
        app.logger.debug(i)
        session = mgr.new_session(auth, system)
        app.logger.debug(session)
        rec = mgr.pull(session, i)
        app.logger.debug(rec)
    except:
        app.logger.exception('Exception in pull')
        return not_found('%s %s' % (sys.exc_type, sys.exc_value))
    return jsonify(create_response(rec))


# auto expire
# This will autoexpire images and cleanup stuck pulls
@app.route('/api/autoexpire/<system>/', methods=["GET"])
def autoexpire(system):
    """ Run the autoexpire handler to purge old images """
    auth = request.headers.get(AUTH_HEADER)
    app.logger.debug("autoexpire system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.autoexpire(session, system)
    except:
        app.logger.exception('Exception in autoexpire')
        return not_found()
    return jsonify({'status': resp})


# expire image
# This will expire an image which removes it from the cache.
@app.route('/api/expire/<system>/<imgtype>/<path:tag>/', methods=["GET"])
def expire(system, imgtype, tag):
    """ Expire a sepcific image for a system """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    memo = "expire system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    app.logger.debug(memo)
    resp = None
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.expire(session, i)
    except:
        app.logger.exception('Exception in expire')
        return not_found()
    return jsonify({'status': resp})


# Show queue
# This will list pull requests and their state
@app.route('/api/queue/<system>/', methods=["GET"])
def queue(system):
    """ List images for a specific system. """
    #auth = request.headers.get(AUTH_HEADER)
    app.logger.debug("show queue system=%s" % (system))
    try:
        session = mgr.new_session(None, system)
        records = mgr.show_queue(session, system)
    except:
        app.logger.exception('Exception in queue')
        return not_found('%s' % (sys.exc_value))
    resp = {'list': records}
    return jsonify(resp)
