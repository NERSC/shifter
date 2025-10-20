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
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, Request, Header, Query
from fastapi.responses import JSONResponse, PlainTextResponse
from fastapi.exceptions import RequestValidationError
import uvicorn
config = {}
mgr = None
logger = logging.getLogger("fastapi.root")

@asynccontextmanager
async def lifespan(app: FastAPI):
    global config
    if 'GWCONFIG' in os.environ:
        CONFIG_FILE = os.environ['GWCONFIG']
    else:
        CONFIG_FILE = '%s/imagemanager.json' % (shifter_imagegw.CONFIG_PATH)
    # Configure logging
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
    global mgr
    mgr = ImageMngr(config, logname="fastapi.root")
    yield
    mgr.shutdown()
#
app = FastAPI(title="Shifter Image Gateway", version="1.0.0", lifespan=lifespan)
AUTH_HEADER = 'authentication'

def getmgr():
    """
    This is intended just for the testing layer so it can shutdown the
    updater thread in the manager.
    """
    return mgr


# For RESTful Service
@app.exception_handler(404)
async def not_found_handler(request: Request, exc: HTTPException):
    """ Standard error function to return a 404. """
    logger.warning("404 return")
    message = {
        'status': 404,
        'error': str(exc.detail),
        'message': 'Not Found: ' + str(request.url),
    }
    return JSONResponse(content=message, status_code=404)


@app.get('/')
async def apihelp():
    """ API helper return """
    return PlainTextResponse("{lookup,pull,expire,list}")


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


# List images
# This will list the images for a system
@app.get('/api/list/{system}')
async def imglist(system: str, authentication: str = Header(None)):
    """ List images for a specific system. """
    auth = authentication
    logger.debug("list system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        records = mgr.imglist(session, system)
        if records is None:
            raise HTTPException(status_code=404, detail='image not found')
    except OSError:
        logger.warning('Bad session or system')
        raise HTTPException(status_code=404, detail='Bad session or system')
    except:
        logger.exception('Unknown Exception in List')
        raise HTTPException(status_code=404, detail='%s' % (sys.exc_value))
    images = []
    for rec in records:
        images.append(create_response(rec))
    resp = {'list': images}
    return resp


# Lookup image
# This will lookup the status of the requested image.
@app.get('/api/lookup/{system}/{imgtype}/{tag:path}')
async def lookup(system: str, imgtype: str, tag: str, authentication: str = Header(None)):
    """ Lookup an image for a system and return its record """
    if (imgtype == "docker" or imgtype == "custom") and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = authentication
    memo = f'lookup system={system} imgtype={imgtype} tag={tag} auth={auth}'
    logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    session = mgr.new_session(auth, system)
    rec = mgr.lookup(session, i)
    if rec is None:
        logger.debug("Image lookup failed.")
        raise HTTPException(status_code=404, detail='image not found')
    logger.info(f"Image lookup {imgtype}:{tag} by user:{session.uid}")
    return create_response(rec)


# Get Metrics
# This will return the most recent XX lookup records.
@app.get('/api/metrics/{system}')
async def metrics(system: str, limit: int = Query(10), authentication: str = Header(None)):
    """ Lookup an image for a system and return its record """
    auth = authentication
    memo = f'metrics system={system} auth={auth}'
    logger.debug(memo)
    try:
        session = mgr.new_session(auth, system)
        recs = mgr.get_metrics(session, system, limit)
    except:
        logger.exception('Exception in metrics')
        raise HTTPException(status_code=404, detail='%s %s' % (sys.exc_type, sys.exc_value))
    return recs


# Pull image
# This will pull the requested image.
@app.post('/api/pull/{system}/{imgtype}/{tag:path}')
async def pull(system: str, imgtype: str, tag: str, request: Request, authentication: str = Header(None)):
    """ Pull a specific image and tag for a systems. """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = authentication
    data = {}
    if len(await request.body()) > 0:
        data = await request.json()

    memo = f"pull system={system} imgtype={imgtype} tag={tag}"
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
    except:
        logger.exception('Exception in pull')
        raise HTTPException(status_code=404, detail=sys.exc_info())
    return create_response(rec)


# Import image
# This will import the requested image from a file path on the system.
@app.post('/api/doimport/{system}/{imgtype}/{tag:path}')
async def doimport(system: str, imgtype: str, tag: str, request: Request, authentication: str = Header(None)):
    """
    Pull a specific image and tag for a systems.
    """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = authentication
    data = {}
    try:
        data = await request.json()
        if data is None:
            data = {}
    except:
        logger.warning("Unable to parse doimport data")
        pass

    memo = "import system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    # Check for path to import file
    if 'filepath' in data:
        i['filepath'] = data['filepath']
    else:
        raise HTTPException(status_code=400, detail="filepath required for direct image import")
    if 'format' in data:
        i['format'] = data['format']
    else:
        msg = "file type (e.g. squashfs) required for direct image import"
        raise HTTPException(status_code=400, detail=msg)
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
        if 'ImportUsers' not in config:
            raise HTTPException(status_code=403, detail="User image import from file disabled.")
        iusers = config['ImportUsers']
        # If ImportUsers is None, no one can do this
        if iusers == "None" or iusers == "none":
            raise HTTPException(status_code=403, detail="User image import from file disabled.")

        # Check if user on approved list
        if len(iusers) > 0 and iusers != "all":
            if user not in iusers:
                msg = "User %s not allowed to import image from file." % (user)
                raise HTTPException(status_code=403, detail=msg)
        rec = mgr.mngrimport(session, i)
        logger.debug(rec)
    except HTTPException:
        raise
    except:
        logger.exception('Exception in import')
        raise HTTPException(status_code=404, detail=sys.exc_info())
    return create_response(rec)


# auto expire
# This will autoexpire images and cleanup stuck pulls
@app.get('/api/autoexpire/{system}')
async def autoexpire(system: str, authentication: str = Header(None)):
    """ Run the autoexpire handler to purge old images """
    auth = authentication
    logger.debug("autoexpire system=%s" % (system))
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.autoexpire(session, system)
    except:
        logger.exception('Exception in autoexpire')
        raise HTTPException(status_code=404, detail='Exception in autoexpire')
    return {'status': resp}


# expire image
# This will expire an image which removes it from the cache.
@app.get('/api/expire/{system}/{imgtype}/{tag:path}')
async def expire(system: str, imgtype: str, tag: str, authentication: str = Header(None)):
    """ Expire a sepcific image for a system """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = '%s:latest' % (tag)

    auth = authentication
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    memo = "expire system=%s imgtype=%s tag=%s" % (system, imgtype, tag)
    logger.debug(memo)
    resp = None
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.expire(session, i)
    except:
        logger.exception('Exception in expire')
        raise HTTPException(status_code=404, detail='Exception in expire')
    return {'status': resp}


# Show queue
# This will list pull requests and their state
@app.get('/api/queue/{system}')
async def queue(system: str):
    """ List images for a specific system. """
    # auth = request.headers.get(AUTH_HEADER)
    logger.debug("show queue system=%s" % (system))
    try:
        session = mgr.new_session(None, system)
        records = mgr.show_queue(session, system)
    except:
        logger.exception('Exception in queue')
        raise HTTPException(status_code=404, detail=sys.exc_info())
    resp = {'list': records}
    return resp

@app.get('/api/status')
def status():
    return "Up"
