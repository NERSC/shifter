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

import logging
from shifter_imagegw.errors import AuthenticationError
from shifter_imagegw.imagemngr import ImageMngr
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, Request, Header, Query
from fastapi.responses import JSONResponse, PlainTextResponse
from pydantic import BaseModel
from shifter_imagegw.config import Config


mgr = None
logger = logging.getLogger("imagegwapi")


@asynccontextmanager
async def lifespan(app: FastAPI):
    global config
    config = Config()
    logger.setLevel(config.LogLevel)
    global mgr
    mgr = ImageMngr(config, logname="fastapi.root")
    yield
    mgr.shutdown()


app = FastAPI(title="Shifter Image Gateway", version="1.0.0",
              lifespan=lifespan)


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
    logger.info("404 return")
    message = {
        'status': 404,
        'error': str(exc.detail),
        'message': f'Not Found: {str(request.url)}',
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
    logger.debug(f"list system={system}")
    try:
        session = mgr.new_session(auth, system)
        records = mgr.imglist(session, system)
        if records is None:
            raise HTTPException(status_code=404, detail='image not found')
    except AuthenticationError as ex:
        logger.warning(f"Auth error {str(ex)}")
        raise HTTPException(status_code=401, detail='Authentication Error')
    except Exception as ex:
        logger.exception('Unknown Exception in List')
        raise HTTPException(status_code=404, detail=str(ex))
    images = []
    for rec in records:
        images.append(create_response(rec))
    resp = {'list': images}
    return resp


# Lookup image
# This will lookup the status of the requested image.
@app.get('/api/lookup/{system}/{imgtype}/{tag:path}')
async def lookup(system: str, imgtype: str, tag: str,
                 authentication: str = Header(None)):
    """ Lookup an image for a system and return its record """
    if (imgtype == "docker" or imgtype == "custom") and tag.find(':') == -1:
        tag = f'{tag}:latest'

    auth = authentication
    memo = f'lookup system={system} imgtype={imgtype} tag={tag} auth={auth}'
    logger.debug(memo)
    try:
        session = mgr.new_session(auth, system)
        i = {'system': system, 'itype': imgtype, 'tag': tag}
        rec = mgr.lookup(session, i)
    except AuthenticationError as ex:
        logger.warning(f"Auth error {str(ex)}")
        raise HTTPException(status_code=401, detail='Authentication Error')
    if rec is None:
        logger.debug("Image lookup failed.")
        raise HTTPException(status_code=404, detail='image not found')
    return create_response(rec)


# Get Metrics
# This will return the most recent XX lookup records.
@app.get('/api/metrics/{system}')
async def metrics(system: str, limit: int = Query(10),
                  authentication: str = Header(None)):
    """ Lookup an image for a system and return its record """
    auth = authentication
    memo = f'metrics system={system} auth={auth}'
    logger.debug(memo)
    try:
        session = mgr.new_session(auth, system)
        recs = mgr.get_metrics(session, system, limit)
    except AuthenticationError as ex:
        logger.warning(f"Auth error {str(ex)}")
        raise HTTPException(status_code=401, detail='Authentication Error')
    except Exception as ex:
        logger.exception('Exception in metrics')
        raise HTTPException(status_code=404, detail=str(ex))
    return recs


class PullRequest(BaseModel):
    allowed_uids: str | None = None
    allowed_gids: str | None = None


# Pull image
# This will pull the requested image.
@app.post('/api/pull/{system}/{imgtype}/{tag:path}')
async def pull(system: str, imgtype: str, tag: str, data: PullRequest | None = None, authentication: str = Header(None)):
    """ Pull a specific image and tag for a systems. """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = f'{tag}:latest'

    auth = authentication
    logger.debug(data)

    logger.debug(f"pull system={system} imgtype={imgtype} tag={tag}")
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    if data and data.allowed_uids:
        # Convert to integers
        i['userACL'] = list(map(lambda x: int(x),
                            data.allowed_uids.split(',')))
    if data and data.allowed_gids:
        # Convert to integers
        i['groupACL'] = list(map(lambda x: int(x),
                             data.allowed_gids.split(',')))
    try:
        logger.debug(i)
        session = mgr.new_session(auth, system)
        logger.debug(session)
        rec = mgr.pull(session, i)
        logger.debug(rec)
    except AuthenticationError as ex:
        logger.warning(f"Auth error {str(ex)}")
        raise HTTPException(status_code=401, detail='Authentication Error')
    except Exception as ex:
        logger.exception('Exception in pull')
        raise HTTPException(status_code=404, detail=str(ex))
    return create_response(rec)


class ImportImage(BaseModel):
    filepath: str
    format: str
    allowed_uids: str | None = None
    allowed_gids: str | None = None


# Import image
# This will import the requested image from a file path on the system.
@app.post('/api/doimport/{system}/{imgtype}/{tag:path}')
async def doimport(system: str, imgtype: str, tag: str, data: ImportImage,
                   authentication: str = Header(None)):
    """
    Pull a specific image and tag for a systems.
    """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = f'{tag}:latest'

    auth = authentication

    memo = f"import system={system} imgtype={imgtype} tag={tag}"
    logger.debug(memo)
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    # Check for path to import file
    i['filepath'] = data.filepath
    i['format'] = data.format

    # Check for list of allowed users or groups
    if data.allowed_uids:
        # Convert to integers
        i['userACL'] = list(map(lambda x: int(x),
                            data.allowed_uids.split(',')))
    if data.allowed_gids:
        # Convert to integers
        i['groupACL'] = list(map(lambda x: int(x),
                             data.allowed_gids.split(',')))
    if not config.ImportUsers:
        raise HTTPException(status_code=403, detail="User image import from file disabled.")

    iusers = config.ImportUsers
    # If ImportUsers is None, no one can do this
    if iusers.lower() == "none":
        raise HTTPException(status_code=403, detail="User image import from file disabled.")

    try:
        session = mgr.new_session(auth, system)
        # only allowed users can import images
        user = session['user']

        # Check if user on approved list
        if len(iusers) > 0 and iusers != "all":
            if user not in iusers:
                msg = f"User {user} not allowed to import image from file."
                raise AuthenticationError(msg)
        rec = mgr.mngrimport(session, i)
        logger.debug(rec)
    except AuthenticationError as ex:
        logger.warning(f"Auth error {str(ex)}")
        raise HTTPException(status_code=401, detail='Authentication Error')
    except Exception as ex:
        logger.exception('Exception in import')
        raise HTTPException(status_code=404, detail=str(ex))
    return create_response(rec)


# auto expire
# This will autoexpire images and cleanup stuck pulls
@app.get('/api/autoexpire/{system}')
async def autoexpire(system: str, authentication: str = Header(None)):
    """ Run the autoexpire handler to purge old images """
    auth = authentication
    logger.debug(f"autoexpire system={system}")
    try:
        session = mgr.new_session(auth, system)
        logger.debug(session)
        resp = mgr.autoexpire(session, system)
    except AuthenticationError as ex:
        logger.warning(f"Auth error {str(ex)}")
        raise HTTPException(status_code=401, detail='Authentication Error')
    except Exception as ex:
        logger.exception('Exception in autoexpire')
        raise HTTPException(status_code=404, detail=str(ex))
    return {'status': resp}


# expire image
# This will expire an image which removes it from the cache.
@app.get('/api/expire/{system}/{imgtype}/{tag:path}')
async def expire(system: str, imgtype: str, tag: str, authentication: str = Header(None)):
    """ Expire a sepcific image for a system """
    if imgtype == "docker" and tag.find(':') == -1:
        tag = f'{tag}:latest'

    auth = authentication
    i = {'system': system, 'itype': imgtype, 'tag': tag}
    memo = f"expire system={system} imgtype={imgtype} tag={tag}"
    logger.debug(memo)
    resp = None
    try:
        session = mgr.new_session(auth, system)
        resp = mgr.expire(session, i)
    except AuthenticationError as ex:
        logger.warning(f"Auth error {str(ex)}")
        raise HTTPException(status_code=401, detail='Authentication Error')
    except Exception as ex:
        logger.exception('Exception in expire')
        raise HTTPException(status_code=404, detail=str(ex))
    return {'status': resp}


# Show queue
# This will list pull requests and their state
@app.get('/api/queue/{system}')
async def queue(system: str):
    """ List images for a specific system. """
    logger.debug(f"show queue system={system}")
    try:
        session = mgr.new_session(None, system)
        records = mgr.show_queue(session, system)
    except Exception as ex:
        logger.exception('Exception in queue')
        raise HTTPException(status_code=404, detail=str(ex))
    resp = {'list': records}
    return resp


@app.get('/api/status')
def status():
    return "Up"
