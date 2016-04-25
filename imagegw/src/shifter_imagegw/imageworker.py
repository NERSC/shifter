from celery import Celery
import json
import os
import time
import dockerv2
import dockerhub
import converters
import transfer
import re
import shutil
import sys
import subprocess
from pymongo import MongoClient
from bson.objectid import ObjectId
import logging
from random import randint

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

CONFIGFILE='imagemanager.json'

queue = None
if 'CONFIG' in os.environ:
    CONFIGFILE=os.environ['CONFIG']

logging.info("Opening %s"%(CONFIGFILE))

with open(CONFIGFILE) as configfile:
    config=json.load(configfile)

# Create Celery Queue and configure serializer
#
queue = Celery('tasks', backend=config['Broker'],broker=config['Broker'])
queue.conf.update(CELERY_ACCEPT_CONTENT = ['json'])
queue.conf.update(CELERY_TASK_SERIALIZER = 'json')
queue.conf.update(CELERY_RESULT_SERIALIZER = 'json')

def initqueue(newconfig):
    """
    This is mainly used by the manager to configure the broker
    after the module is already loaded
    """
    global queue, config
    config=newconfig
    queue = Celery('tasks', backend=config['Broker'],broker=config['Broker'])
    queue.conf.update(CELERY_ACCEPT_CONTENT = ['json'])
    queue.conf.update(CELERY_TASK_SERIALIZER = 'json')
    queue.conf.update(CELERY_RESULT_SERIALIZER = 'json')



def normalized_name(request):
    """
    Helper function that returns a filename based on the request
    """
    return '%s_%s'%(request['itype'],request['tag'].replace('/','_'))
    #return request['meta']['id']

def pull_image(request):
    """
    pull the image down and extract the contents

    Returns True on success
    """
    dir=os.getcwd()
    cdir=config['CacheDirectory']
    edir=config['ExpandDirectory']
    parts=re.split('[:/]',request['tag'])
    if len(parts)==3:
        (location,repo,tag)=parts
        if location.find('.')<0:
            # This is a dockerhub repo with a username
            repo='%s/%s'%(location,repo)
            location=config['DefaultImageLocation']
    elif len(parts)==2:
        (repo,tag)=parts
        location=config['DefaultImageLocation']
    else:
        raise OSError('Unable to parse tag %s'%request['tag'])
    logging.debug("doing image pull for %s %s %s"%(location,repo,tag))
    cacert=None
    if location in config['Locations']:
        params=config['Locations'][location]
        rtype=params['remotetype']
        if 'sslcacert' in params:
            if params['sslcacert'].startswith('/'):
              cacert=params['sslcacert']
            else:
              cacert='%s/%s'%(dir,params['sslcacert'])
            if not os.path.exists(cacert):
                raise OSError('%s does not exist'%(cacert))
    else:
        raise KeyError('%s not found in configuration'%(location))
    if rtype=='dockerv2':
        try:
            resp=dockerv2.pullImage(None, 'https://%s'%(location),
                repo, tag,
                cachedir=cdir,expanddir=edir,
                cacert=cacert)
            request['meta']=resp
            request['expandedpath']=resp['expandedpath']
            return True
        except:
            logging.warn(sys.exc_value)
            return False
    elif rtype=='dockerhub':
        logging.debug("pulling from docker hub %s %s"%(repo,tag))
        try:
            resp=dockerhub.pullImage(None, None,
                repo, tag,
                cachedir=cdir,expanddir=edir,
                cacert=cacert)
            request['meta']=resp
            request['expandedpath']=resp['expandedpath']
            return True
        except:
            logging.warn(sys.exc_value)
            raise

    else:
        raise NotImplementedError('Unsupported remote type %s'%(rtype))
    return False

def examine_image(request):
    """
    examine the image

    Returns True on success
    """
    # TODO: Add checks to examine the image.  Should be extensible.
    return True

def convert_image(request):
    """
    Convert the image to the required format for the target system

    Returns True on success
    """
    system=request['system']
    format=config['DefaultImageFormat']
    if format in request:
        format=request['format']
    else:
        request['format']=format
    cdir=config['CacheDirectory']
    imagefile='%s.%s'%(request['expandedpath'],format)
    status=converters.convert(format,request['expandedpath'],imagefile)

    # Write Metadata file
    request['imagefile']=imagefile
    return status

def write_metadata(request):
    """
    Write out the metadata file

    Returns True on success
    """
    format=request['format']
    meta=request['meta']
    metafile='%s.meta'%(request['expandedpath'])
    status=converters.writemeta(format,meta,metafile)

    # Write Metadata file
    request['metafile']=metafile
    return status


def transfer_image(request):
    """
    Transfers the image to the target system based on the configuration.

    Returns True on success
    """
    system=request['system']
    if system not in config['Platforms']:
        raise KeyError('%s is not in the configuration'%system)
    sys=config['Platforms'][system]
    meta=None
    if 'metafile' in request:
        meta=request['metafile']
    return transfer.transfer(sys,request['imagefile'],meta)

def cleanup_temporary(request):
    items = ('expandedpath', 'imagefile', 'metafile')
    for item in items:
        if item not in request or request[item] is None:
            continue
        cleanitem = request[item]
        if type(cleanitem) is unicode:
            cleanitem = str(cleanitem)

        if type(cleanitem) is not str:
            raise ValueError('Invalid type for %s, %s' % (item, type(cleanitem)))
        if cleanitem == '' or cleanitem == '/':
            raise ValueError('Invalid value for %s: %s' % (item, cleanitem))
        if not cleanitem.startswith(config['ExpandDirectory']):
            raise ValueError('Invalid location for %s: %s' % (item, cleanitem))
        if os.path.exists(cleanitem):
            logging.info("Worker: removing %s" % cleanitem)
            subprocess.call(['chmod', '-R', 'u+w', cleanitem])
            if os.path.isdir(cleanitem):
                shutil.rmtree(cleanitem)
            else:
                os.unlink(cleanitem)

@queue.task(bind=True)
def dopull(self,request,TESTMODE=0):
    """
    Celery task to do the full workflow of pulling an image and transferring it
    """
    logging.debug("dopull system=%s tag=%s"%(request['system'],request['tag']))
    if TESTMODE==1:
        for state in ('PULLING','EXAMINATION','CONVERSION','TRANSFER','READY'):
            logging.info("Worker: TESTMODE Updating to %s"%(state))
            self.update_state(state=state)
            time.sleep(1)
        id='%x'%(randint(0,100000))
        return {'id':id,'entrypoint':['./blah'],'workdir':'/root','env':['FOO=bar','BAZ=boz']}
    elif TESTMODE==2:
        logging.info("Worker: TESTMODE 2 setting failure")
        raise OSError('task failed')
    try:
        # Step 1 - Do the pull
        self.update_state(state='PULLING')
        if not pull_image(request):
            logging.info("Worker: Pull failed")
            raise OSError('Pull failed')
        if 'meta' not in request:
            raise OSError('Metadata not populated')
        # Step 2 - Check the image
        self.update_state(state='EXAMINATION')
        if not examine_image(request):
            raise OSError('Examine failed')
        # Step 3 - Convert
        self.update_state(state='CONVERSION')
        if not convert_image(request):
            raise OSError('Conversion failed')
        if not write_metadata(request):
            raise OSError('Metadata creation failed')
        # Step 4 - TRANSFER
        self.update_state(state='TRANSFER')
        if not transfer_image(request):
            raise OSError('Transfer failed')
        # Done
        self.update_state(state='READY')
        cleanup_temporary(request)
        return request['meta']

    except:
        logging.error("ERROR: dopull failed system=%s tag=%s"%(request['system'],request['tag']))
        self.update_state(state='FAILURE')
        cleanup_temporary(request)
        raise
