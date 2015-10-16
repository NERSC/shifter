#!/usr/bin/python

import json
import os
import sys
import socket
import time
import imagemngr
from flask import Flask, Blueprint, request, Response, url_for, jsonify

"""
Shifter, Copyright (c) 2015, The Regents of the University of California,
through Lawrence Berkeley National Laboratory (subject to receipt of any
required approvals from the U.S. Dept. of Energy).  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
 3. Neither the name of the University of California, Lawrence Berkeley
    National Laboratory, U.S. Dept. of Energy nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.`

See LICENSE for full text.
"""

imagegwapi = Flask(__name__)
imagegwapi.debug_log_format= '%(asctime)s %(levelname)s: %(message)s [in %(pathname)s:%(lineno)d]'

# Default Configuration
DEBUG_FLAG = True
LISTEN_PORT = 5000
AUTH_HEADER='authentication'

CONFIG='imagemanager.json'
if 'GWCONFIG' in os.environ:
    CONFIG=os.environ['GWCONFIG']

# For RESTful Service
@imagegwapi.errorhandler(404)
def not_found(error=None):
    imagegwapi.logger.warning("404 return")
    message = {
            'status': 404,
            'error': str(error),
            'message': 'Not Found: ' + request.url,
    }
    resp = jsonify(message)
    resp.status_code = 404
    print "ERROR: %s"%str(error)
    return resp

@imagegwapi.route('/')
def help():
    return "{lookup,pull,expire}"

def create_response(rec):
    resp={'id':'TODO'}
    for field in ('system','itype','tag','status','userAcl','groupAcl','ENV','ENTRY'):
        try:
            resp[field]=rec[field]
        except KeyError,e:
            resp[field]='MISSING'
    return resp

# Lookup image
# This will lookup the status of the requested image.
@imagegwapi.route('/api/lookup/<system>/<type>/<path:tag>/', methods=["GET"])
def lookup(system,type,tag):
    auth=request.headers.get(AUTH_HEADER)
    imagegwapi.logger.debug("lookup system=%s type=%s tag=%s auth=%s"%(system,type,tag,auth))
    i={'system':system,'itype':type,'tag':tag}
    try:
        session=mgr.new_session(auth,system)
        rec=mgr.lookup(session,i)
        if rec==None:
            return not_found('image not found')
    except:
        imagegwapi.logger.error(sys.exc_value)
        return not_found('%s'%(sys.exc_value))
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
@imagegwapi.route('/api/pull/<system>/<type>/<path:tag>/', methods=["POST"])
def pull(system,type,tag):
    auth=request.headers.get(AUTH_HEADER)
    imagegwapi.logger.debug("pull system=%s type=%s tag=%s"%(system,type,tag))
    i={'system':system,'itype':type,'tag':tag}
    try:
        session=mgr.new_session(auth,system)
        id=mgr.pull(session,i)
        rec=mgr.lookup(session,i)
        imagegwapi.logger.debug(rec)
    except:
        return not_found(sys.exc_value)
    return jsonify(create_response(rec))

# expire image
# This will expire an image which removes it from the cache.
@imagegwapi.route('/api/expire/<system>/<type>/<tag>/<id>/', methods=["GET"])
def expire(system,type,tag,id):
    auth=request.headers.get(AUTH_HEADER)
    imagegwapi.logger.debug("expire system=%s type=%s tag=%s"%(system,type,tag))
    try:
        resp=mgr.expire(auth,system,type,tag,id)
    except:
        return not_found()
    return jsonify(resp)


#
# Initialization
with open(CONFIG) as config_file:
    config = json.load(config_file)
mgr=imagemngr.imagemngr(CONFIG,logger=imagegwapi.logger)


if __name__ == '__main__':
    imagegwapi.run(debug=DEBUG_FLAG, host='0.0.0.0', port=LISTEN_PORT, threaded=True)
