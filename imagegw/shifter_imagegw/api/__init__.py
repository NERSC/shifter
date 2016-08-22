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

import json
import os
import sys
import socket
import time
import shifter_imagegw
from shifter_imagegw import imagemngr
import logging
from flask import Flask, Blueprint, request, Response, url_for, jsonify

app = Flask(__name__)
config = {}
DEBUG_FLAG = True
LISTEN_PORT = 5000
AUTH_HEADER = 'authentication'


if 'GWCONFIG' in os.environ:
    CONFIG_FILE = os.environ['GWCONFIG']
else:
    CONFIG_FILE = '%s/imagemanager.json' % (shifter_imagegw.CONFIG_PATH)

def _get_log_handler():
    handler = logging.StreamHandler()
    fmt = '%(asctime)s [%(name)s] %(levelname)s : %(message)s'
    handler.setFormatter(logging.Formatter(fmt))
    handler.setLevel(logging.DEBUG)
    return handler

app.logger.setLevel(logging.INFO)
app.logger.addHandler(_get_log_handler())
app.logger.debug('Initializing image manager')

app.logger.info("initializing with %s" % (CONFIG_FILE))
with open(CONFIG_FILE) as config_file:
    config = json.load(config_file)
mgr = imagemngr.imagemngr(config, logname='imagegwapi')
print mgr
__all__ = [app, mgr, config, AUTH_HEADER, DEBUG_FLAG, LISTEN_PORT]

# For RESTful Service
@app.errorhandler(404)
def not_found(error=None):
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
    return "{lookup,pull,expire}"

def create_response(rec):
    resp = {}
    fields = (
        'id', 'system', 'itype', 'tag', 'status', 'userAcl', 'groupAcl',
        'ENV', 'ENTRY', 'WORKDIR', 'last_pull', 'status_message',
    )
    for field in fields:
        try:
            resp[field] = rec[field]
        except KeyError:
            resp[field] = 'MISSING'
    return resp

# Lookup image
# This will lookup the status of the requested image.
@app.route('/api/list/<system>/', methods=["GET"])
def imglist(system):
    auth = request.headers.get(AUTH_HEADER)
    app.logger.debug("list system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        records = mgr.imglist(session, system)
        if records == None:
            return not_found('image not found')
    except:
        app.logger.exception('Exception in list')
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
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    memo = 'lookup system=%s imgtype=%s tag=%s auth=%s' \
            % (system, imgtype, tag, auth)
    app.logger.debug(memo)
    i = {'system':system, 'itype':imgtype, 'tag':tag}
    try:
        session = mgr.new_session(auth, system)
        rec = mgr.lookup(session, i)
        if rec == None:
            return not_found('image not found')
    except:
        app.logger.exception('Exception in lookup')
        return not_found('%s %s' % (sys.exc_type, sys.exc_value))
    return jsonify(create_response(rec))
# {
# 	"tag": "ubuntu:15.04",
# 	"id": "<long random string of hexadecimal characters>",
# 	"system": "edison",
# 	"status": "ready",
# 	"userAcl": [],
# 	"groupAcl": [],
# 	"ENV": [
# 		"PATH=/usr/bin:bin"
# 	],
# 	"ENTRY": ""
# }

# Pull image
# This will pull the requested image.
@app.route('/api/pull/<system>/<imgtype>/<path:tag>/', methods=["POST"])
def pull(system, imgtype, tag):
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    memo = "pull system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    app.logger.debug(memo)
    i = {'system':system, 'itype':imgtype, 'tag':tag}
    try:
        session = mgr.new_session(auth, system)
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
    auth = request.headers.get(AUTH_HEADER)
    app.logger.debug("expire system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.autoexpire(session, system)
    except:
        app.logger.exception('Exception in autoexpire')
        return not_found()
    return jsonify({'status': resp})

# expire image
# This will expire an image which removes it from the cache.
@app.route('/api/expire/<system>/<imgtype>/<tag>/', methods=["GET"])
def expire(system, imgtype, tag):
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    i = {'system':system, 'itype':imgtype, 'tag':tag}
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
