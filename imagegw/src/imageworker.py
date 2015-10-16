from celery import Celery
import json
import os
import time
import dockerv2
import dockerhub
import converters
import transfer
import re
import sys
from pymongo import MongoClient
from bson.objectid import ObjectId
import logging

CONFIGFILE='imagemanager.json'
DEBUG=0

#if 'TESTMODE' in os.environ:
#    print "Setting Testmode"
#    TESTMODE=os.environ['TESTMODE']
if 'CONFIG' in os.environ:
    CONFIGFILE=os.environ['CONFIG']

if DEBUG:
  logging.debug("Opening %s"%(CONFIGFILE))

with open(CONFIGFILE) as configfile:
    config=json.load(configfile)

# Create Celery Queue and configure serializer
#
queue = Celery('tasks', backend=config['Broker'],broker=config['Broker'])
queue.conf.update(CELERY_ACCEPT_CONTENT = ['json'])
queue.conf.update(CELERY_TASK_SERIALIZER = 'json')
queue.conf.update(CELERY_RESULT_SERIALIZER = 'json')

#status: 	"uptodate", "enqueued", "transferFromRemote", "examination",
#"formatConversion", "transferToPlatform", "error"

def normalized_name(request):
    """
    Helper function that returns a filename based on the request
    """
    return '%s_%s'%(request['itype'],request['tag'].replace('/','_'))

def pull_image(request):
    """
    pull the image down and extract the contents

    Returns True on success
    """
    dir=os.getcwd()
    cdir=config['CacheDirectory']
    edir=os.path.join(cdir,normalized_name(request))
    request['expandedpath']=edir
    parts=re.split('[:/]',request['tag'])
    if len(parts)==3:
        (location,repo,tag)=parts
    elif len(parts)==2:
        (repo,tag)=parts
        location='index.docker.io'
    else:
        raise OSError('Unable to parse tag %s'%request['tag'])
    logging.debug("doing image pull for %s %s %s"%(location,repo,tag))
    cacert=None
    if location in config['Locations']:
        params=config['Locations'][location]
        rtype=params['remotetype']
        if 'sslcacert' in params:
            cacert='%s/%s'%(dir,params['sslcacert'])
            if not os.path.exists(cacert):
                raise OSError('%s does not exist'%(cacert))
    else:
        raise KeyError('%s not found in configuration'%(location))
    if rtype=='dockerv2':
        try:
            ipath=dockerv2.pullImage(None, 'https://%s'%(location),
                repo, tag,
                cachedir=cdir,expanddir=edir,
                cacert=cacert)
            return True
        except:
            logging.warn(sys.exc_value)
            return False
    elif rtype=='dockerhub':
        logging.debug("pulling from docker hub %s %s"%(repo,tag))
        try:
            ipath=dockerhub.pullImage(None, None,
                repo, tag,
                cachedir=cdir,expanddir=edir,
                cacert=cacert)
            return True
        except:
            logging.warn(sys.exc_value)
            return False

    else:
        raise NotImplementedError('Unsupported remote type %s'%(rtype))
    return False

def examine_image(request):
    """
    examine the image - TODO

    Returns True on success
    """
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
    cdir=config['CacheDirectory']
    imagefile=os.path.join(cdir,normalized_name(request)+'.'+format)
    status=converters.convert(format,request['expandedpath'],imagefile)
    request['imagefile']=imagefile
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
    return transfer.transfer(sys,request['imagefile'])

@queue.task(bind=True)
def dopull(self,request,TESTMODE=0):
    """
    Celery task to do the full workflow of pulling an image and transferring it
    """
    logging.info("DEBUG: dopull system=%s tag=%s"%(request['system'],request['tag']))
    if TESTMODE==1:
        for state in ('PULLING','EXAMINATION','CONVERSION','TRANSFER','READY'):
            logging.info("Worker: TESTMODE Updating to %s"%(state))
            self.update_state(state=state)
            time.sleep(1)
        return
    elif TESTMODE==2:
        logging.info("Worker: TESTMODE 2 setting failure")
        raise OSError('task failed')
    try:
        # Step 1 - Do the pull
        self.update_state(state='PULLING')
        if not pull_image(request):
            raise OSError('Pull failed')
        # Step 2 - Check the image
        self.update_state(state='EXAMINATION')
        if not examine_image(request):
            raise OSError('Examine failed')
        # Step 3 - Convert
        self.update_state(state='CONVERSION')
        if not convert_image(request):
            raise OSError('Conversion failed')
        # Step 4 - TRANSFER
        self.update_state(state='TRANSFER')
        if not transfer_image(request):
            raise OSError('Transfer failed')
        # Done
        self.update_state(state='READY')
    except:
        logging.error("ERROR: dopull failed system=%s tag=%s"%(request['system'],request['tag']))
        self.update_state(state='FAILURE')
        raise OSError("Pull Failed")
