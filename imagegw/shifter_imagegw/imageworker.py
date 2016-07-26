from celery import Celery
import json
import os
from time import time,sleep
import shifter_imagegw
import dockerv2
import converters
import transfer
import re
import shutil
import sys
import subprocess
from pymongo import MongoClient
from bson.objectid import ObjectId
from random import randint
import logging


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


queue = None
if 'GWCONFIG' in os.environ:
    CONFIGFILE=os.environ['GWCONFIG']
else:
    CONFIGFILE='%s/imagemanager.json' % (shifter_imagegw.configPath)

logging.info("Opening %s"%(CONFIGFILE))

with open(CONFIGFILE) as configfile:
    config=json.load(configfile)

if 'CacheDirectory' in config:
    if not os.path.exists(config['CacheDirectory']):
        os.mkdir(config['CacheDirectory'])
if 'ExpandDirectory' in config:
    if not os.path.exists(config['ExpandDirectory']):
        os.mkdir(config['ExpandDirectory'])


# Create Celery Queue and configure serializer
#
queue = Celery('tasks', backend=config['Broker'],broker=config['Broker'])
queue.conf.update(CELERY_ACCEPT_CONTENT = ['json'])
queue.conf.update(CELERY_TASK_SERIALIZER = 'json')
queue.conf.update(CELERY_RESULT_SERIALIZER = 'json')

class updater():
    def __init__(self,update_state):
        self.update_state=update_state

    def update_status(self,state,message):
        if self.update_state is not None:
            self.update_state(state=state,meta={'heartbeat':time(),'message':message})

defupdater=updater(None)

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


def already_processed(request):
    return False

def pull_image(request,updater=defupdater):
    """
    pull the image down and extract the contents

    Returns True on success
    """
    dir=os.getcwd()
    cdir=config['CacheDirectory']
    edir=config['ExpandDirectory']
    # See if there is a location specified
    location=config['DefaultImageLocation']
    tag=request['tag']
    if tag.find('/')>0:
      parts=tag.split('/')
      if parts[0] in config['Locations']:
        # This is a location
        location=parts[0]
        tag='/'.join(parts[1:])

    parts=tag.split(':')
    if len(parts)==2:
        (repo,tag)=parts
    else:
        raise OSError('Unable to parse tag %s'%request['tag'])
    logging.debug("doing image pull for loc=%s repo=%s tag=%s"%(location,repo,tag))
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
        url='https://%s'%(location)
        if 'url' in params:
          url=params['url']
        try:
            #resp=dockerv2.pullImage(None, url,
            #    repo, tag,
            #    cachedir=cdir,expanddir=edir,
            #    cacert=cacert,logger=logger)
            options={}
            if cacert is not None:
                options['cacert'] = cacert
            options['baseUrl'] = url
            imageident = '%s:%s' % (repo, tag)
            dh = dockerv2.dockerv2Handle(imageident, options,updater=updater)
            updater.update_status("PULLING",'Getting manifest')
            manifest = dh.getImageManifest()
            resp=dh.pull_layers(manifest,cdir)
            expandedpath=os.path.join(edir,str(resp['id']))
            if not os.path.exists(expandedpath):
                os.mkdir(expandedpath)
            updater.update_status("PULLING",'Extracting Layers')
            dh.extractDockerLayers(expandedpath, dh.get_eldest(), cachedir=cdir)
            request['meta']=resp
            request['expandedpath']=expandedpath
            return True
        except:
            logging.warn(sys.exc_value)
            raise
    elif rtype=='dockerhub':
        logging.warning("Use of depcreated dockerhub type")
        raise NotImplementedError('dockerhub type is depcreated.  Use dockverv2')
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
    return transfer.transfer(sys,request['imagefile'],meta, logging)

def remove_image(request):
    """
    Remove the image to the target system based on the configuration.

    Returns True on success
    """
    system=request['system']
    if system not in config['Platforms']:
        raise KeyError('%s is not in the configuration'%system)
    sys=config['Platforms'][system]
    imagefile=request['id']+'.'+request['format']
    meta=request['id']+'.meta'
    if 'metafile' in request:
        meta=request['metafile']
    return transfer.remove(sys,imagefile,meta, logging)


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
            try:
                subprocess.call(['chmod', '-R', 'u+w', cleanitem])
                if os.path.isdir(cleanitem):
                    shutil.rmtree(cleanitem,ignore_errors=True)
                else:
                    os.unlink(cleanitem)
            except:
                logging.error("Worker: caught exception while trying to clean up (%s) %s." % (item,cleanitem))
                #logging.warn(sys.exc_value)
                pass



@queue.task(bind=True)
def dopull(self,request,TESTMODE=0):
    """
    Celery task to do the full workflow of pulling an image and transferring it
    """
    logging.debug("dopull system=%s tag=%s"%(request['system'],request['tag']))
    us=updater(self.update_state)
    if TESTMODE==1:
        for state in ('PULLING','EXAMINATION','CONVERSION','TRANSFER','READY'):
            logging.info("Worker: TESTMODE Updating to %s"%(state))
            us.update_status(state,state)
            sleep(1)
        id='%x'%(randint(0,100000))
        return {'id':id,'entrypoint':['./blah'],'workdir':'/root','env':['FOO=bar','BAZ=boz']}
    elif TESTMODE==2:
        logging.info("Worker: TESTMODE 2 setting failure")
        raise OSError('task failed')
    try:
        # Step 1 - Do the pull
        us.update_status('PULLING','PULLING')
        print "pulling image %s"%(request['tag'])
        if not pull_image(request,updater=us):
            print "pull_image failed"
            logging.info("Worker: Pull failed")
            raise OSError('Pull failed')
        if 'meta' not in request:
            raise OSError('Metadata not populated')
        # Step 2 - Check the image
        us.update_status('EXAMINATION','Examining image')
        print "Worker: examining image %s"%(request['tag'])
        if not examine_image(request):
            raise OSError('Examine failed')
        # Step 3 - Convert
        us.update_status('CONVERSION','Converting image')
        print "Worker: converting image %s"%(request['tag'])
        if not convert_image(request):
            raise OSError('Conversion failed')
        if not write_metadata(request):
            raise OSError('Metadata creation failed')
        # Step 4 - TRANSFER
        us.update_status('TRANSFER','Transferring image')
        logging.info("Worker: transferring image %s"%(request['tag']))
        print "Worker: transferring image %s"%(request['tag'])
        if not transfer_image(request):
            raise OSError('Transfer failed')
        # Done
        us.update_status('READY','Image ready')
        cleanup_temporary(request)
        return request['meta']

    except:
        logging.error("ERROR: dopull failed system=%s tag=%s"%(request['system'],request['tag']))
        print sys.exc_value
        self.update_state(state='FAILURE')
        #cleanup_temporary(request)
        raise


@queue.task(bind=True)
def doexpire(self,request,TESTMODE=0):
    """
    Celery task to do the full workflow of pulling an image and transferring it
    """
    logging.debug("do expire system=%s tag=%s TM=%d"%(request['system'],request['tag'],TESTMODE))
    try:
        self.update_state(state='EXPIRING')
        if not remove_image(request):
            logging.info("Worker: Expire failed")
            raise OSError('Expire failed')

        self.update_state(state='EXPIRED')
        return True

    except:
        logging.error("ERROR: doexpire failed system=%s tag=%s"%(request['system'],request['tag']))
        raise
