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
from sanic import Sanic
from sanic.log import logger
from sanic import response
from sanic.response import json as jsonify
from sanic.exceptions import NotFound

app = Sanic("shifter", strict_slashes=True)
config = {}
AUTH_HEADER = 'authentication'


if 'GWCONFIG' in os.environ:
    CONFIG_FILE = os.environ['GWCONFIG']
else:
    CONFIG_FILE = '%s/imagemanager.json' % (shifter_imagegw.CONFIG_PATH)

logger.debug('Initializing api image manager')

logger.info("initializing with %s" % (CONFIG_FILE))
with open(CONFIG_FILE) as config_file:
    config = json.load(config_file)
    if 'LogLevel' in config:
        LOG_STRING = config['LogLevel'].lower()
        if LOG_STRING == 'debug':
            logger.setLevel(logging.DEBUG)
        elif LOG_STRING == 'info':
            logger.setLevel(logging.INFO)
        elif LOG_STRING == 'warn':
            logger.setLevel(logging.WARN)
        elif LOG_STRING == 'error':
            logger.setLevel(logging.ERROR)
        elif LOG_STRING == 'critical':
            logger.setLevel(logging.CRITICAL)
        else:
            logger.critical('Unrecongnized Log Level specified')
mgr = ImageMngr(config, logname="sanic.root")


def getmgr():
    """
    This is intended just for the testing layer so it can shutdown the
    updater thread in the manager.
    """
    return mgr


# For RESTful Service
# @app.errorhandler(404)
@app.exception(NotFound)
def not_found(request, error=None):
    """ Standard error function to return a 404. """
    logger.warning("404 return")
    message = {
        'status': 404,
        'error': str(error),
        'message': 'Not Found: ' + request.url,
    }
    resp = jsonify(message, status=404)
    # resp.status_code = 404
    return resp


@app.route('/')
def apihelp(request):
    """ API helper return """
    return response.text("{lookup,pull,expire,list}")


def create_response(rec):
    """ Helper function to create a formated JSON response. """
    resp = {}
    fields = (
        'id', 'system', 'itype', 'tag', 'status', 'userACL', 'groupACL',
        'ENV', 'ENTRY', 'WORKDIR', 'LABELS', 'last_pull', 'status_message',
    )
    for field in fields:
        try:
            resp[field] = rec[field]
        except KeyError:
            resp[field] = 'MISSING'
    return resp


def decode_path(pth):
    """
    Convert a URL encoded path.  We just
    see a few outliers currently
    """
    return pth.replace("%3A", ":")


# List images
# This will list the images for a system
@app.route('/api/list/<system>/', methods=["GET"])
def imglist(request, system):
    """ List images for a specific system. """
    auth = request.headers.get(AUTH_HEADER)
    logger.debug("list system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        records = mgr.imglist(session, system)
        if records is None:
            return not_found(request, 'image not found')
    except OSError:
        logger.warning('Bad session or system')
        return not_found(request, 'Bad session or system')
    except Exception:
        logger.exception('Unknown Exception in List')
        return not_found(request, '%s' % (sys.exc_value))
    images = []
    for rec in records:
        images.append(create_response(rec))
    resp = {'list': images}
    return jsonify(resp)


# Lookup image
# This will lookup the status of the requested image.
@app.route('/api/lookup/<system>/<imgtype>/<tag:path>/', methods=["GET"])
def lookup(request, system, imgtype, tag):
    """ Lookup an image for a system and return its record """
    tag = decode_path(tag)
    if (imgtype == "docker" or imgtype == "custom") and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    memo = 'lookup system=%s imgtype=%s tag=%s auth=%s' \
           % (system, imgtype, tag, auth)
    logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    try:
        session = mgr.new_session(auth, system)
        rec = mgr.lookup(session, i)
        if rec is None:
            logger.debug("Image lookup failed.")
            return not_found(request, 'image not found')
    except Exception:
        logger.exception('Exception in lookup')
        return not_found(request, '%s %s' % (sys.exc_type, sys.exc_value))
    return jsonify(create_response(rec))


# Get Metrics
# This will return the most recent XX lookup records.
@app.route('/api/metrics/<system>/', methods=["GET"])
def metrics(request, system):
    """ Lookup an image for a system and return its record """
    auth = request.headers.get(AUTH_HEADER)
    memo = 'metrics system=%s auth=%s' \
           % (system, auth)
    logger.debug(memo)
    limit = int(request.args.get('limit', '10'))
    try:
        session = mgr.new_session(auth, system)
        recs = mgr.get_metrics(session, system, limit)
    except Exception:
        logger.exception('Exception in metrics')
        return not_found(request, '%s %s' % (sys.exc_type, sys.exc_value))
    return jsonify(recs)


# Pull image
# This will pull the requested image.
@app.route('/api/pull/<system>/<imgtype>/<tag:path>/', methods=["POST"])
def pull(request, system, imgtype, tag):
    """ Pull a specific image and tag for a systems. """
    tag = decode_path(tag)
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    data = {}
    try:
        data = request.json
        if data is None:
            data = {}
    except Exception:
        data = request.get_data()
        logger.warn(f"Unable to parse pull data '{data}'")
        pass

    memo = "pull system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    if 'allowed_uids' in data:
        # Convert to integers
        i['userACL'] = list(map(lambda x: int(x),
                            data['allowed_uids'].split(',')))
    if 'allowed_gids' in data:
        # Convert to integers
        i['groupACL'] = list(map(lambda x: int(x),
                             data['allowed_gids'].split(',')))
    try:
        logger.debug(i)
        session = mgr.new_session(auth, system)
        logger.debug(session)
        rec = mgr.pull(session, i)
        logger.debug(rec)
    except Exception:
        logger.exception('Exception in pull')
        return not_found(request, '%s %s' % (sys.exc_type, sys.exc_value))
    return jsonify(create_response(rec))


# Import image
# This will import the requested image from a file path on the system.
@app.route('/api/doimport/<system>/<imgtype>/<tag:path>/', methods=["POST"])
def doimport(request, system, imgtype, tag):
    """
    Pull a specific image and tag for a systems.
    """
    tag = decode_path(tag)
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    data = {}
    try:
        data = request.json
        if data is None:
            data = {}
    except Exception:
        logger.warn("Unable to parse doimport data '%s'" %
                    (request.text))
        pass

    memo = "import system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    # Check for path to import file
    if 'filepath' in data:
        i['filepath'] = data['filepath']
    else:
        raise OSError("filepath required for direct image import")
    if 'format' in data:
        i['format'] = data['format']
    else:
        msg = "file type (e.g. squashfs) required for direct image import"
        raise OSError(msg)
    # Check for list of allowed users or groups
    if 'allowed_uids' in data:
        # Convert to integers
        i['userACL'] = list(map(lambda x: int(x),
                            data['allowed_uids'].split(',')))
    if 'allowed_gids' in data:
        # Convert to integers
        i['groupACL'] = list(map(lambda x: int(x),
                             data['allowed_gids'].split(',')))
    try:
        session = mgr.new_session(auth, system)
        # only allowed users can import images
        user = session['user']
        if 'ImportUsers' not in config.keys():
            raise OSError("User image import from file disabled.")
        iusers = config['ImportUsers']
        # If ImportUsers is None, no one can do this
        if iusers == "None" or iusers == "none":
            raise OSError("User image import from file disabled.")

        # Check if user on approved list
        if len(iusers) > 0 and iusers != "all":
            if user not in iusers:
                msg = "User %s not allowed to import image from file." % (user)
                raise OSError(msg)
        rec = mgr.mngrimport(session, i)
        logger.debug(rec)
    except Exception:
        logger.exception('Exception in import')
        return not_found(request, '%s %s' % (sys.exc_type, sys.exc_value))
    return jsonify(create_response(rec))


# auto expire
# This will autoexpire images and cleanup stuck pulls
@app.route('/api/autoexpire/<system>/', methods=["GET"])
def autoexpire(request, system):
    """ Run the autoexpire handler to purge old images """
    auth = request.headers.get(AUTH_HEADER)
    logger.debug("autoexpire system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.autoexpire(session, system)
    except Exception:
        logger.exception('Exception in autoexpire')
        return not_found(request)
    return jsonify({'status': resp})


# expire image
# This will expire an image which removes it from the cache.
@app.route('/api/expire/<system>/<imgtype>/<tag:path>/', methods=["GET"])
def expire(request, system, imgtype, tag):
    """ Expire a sepcific image for a system """
    tag = decode_path(tag)
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = request.headers.get(AUTH_HEADER)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    memo = "expire system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    logger.debug(memo)
    resp = None
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.expire(session, i)
    except Exception:
        logger.exception('Exception in expire')
        return not_found(request)
    return jsonify({'status': resp})


# Show queue
# This will list pull requests and their state
@app.route('/api/queue/<system>/', methods=["GET"])
def queue(request, system):
    """ List images for a specific system. """
    # auth = request.headers.get(AUTH_HEADER)
    logger.debug("show queue system=%s" % (system))
    try:
        session = mgr.new_session(None, system)
        records = mgr.show_queue(session, system)
    except Exception:
        logger.exception('Exception in queue')
        return not_found(request, '%s' % (sys.exc_value))
    resp = {'list': records}
    return jsonify(resp)


if __name__ == "__main__":
    workers = int(os.environ.get("WORKERS", 1))
    app.run(host="0.0.0.0", port=8000, workers=workers)
