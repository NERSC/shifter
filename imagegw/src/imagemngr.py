#!/usr/bin/python

from pymongo import MongoClient
import json
from imageworker import dopull
import bson
import celery
import sys, os
import logging
import auth
from time import time

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



class imagemngr:
  """
  This class handles most of the backend work for the image gateway.
  It uses a Mongo Database to track state, uses celery to dispatch work,
  and has public functions to lookup, pull and expire images.
  """

  def __init__(self, CONFIGFILE, logname='imagemngr'):
      """
      Create an instance of the image manager.
      """
      logformat='%(asctime)s %(levelname)s: %(message)s [in %(pathname)s:%(lineno)d]'
      self.logger = logging.getLogger(logname)
      #self.logger.setLevel(logging.DEBUG)
      #ch = logging.StreamHandler()
      #formatter = logging.Formatter('%(asctime)s [%(name)s] %(levelname)s : %(message)s')
      #ch.setFormatter(formatter)
      #ch.setLevel(logging.DEBUG)
      #self.logger.addHandler(ch)
      #self.logger.debug('Initializing image manager')
      with open(CONFIGFILE) as config_file:
          self.config = json.load(config_file)
      if 'Platforms' not in self.config:
          raise NameError('Platforms not defined')
      self.systems=[]
      self.tasks=[]
      self.task_image_id=dict()
      # This is not intended to provide security, but just
      # provide a basic check that a session object is correct
      self.magic='imagemngrmagic'
      if 'Authentication' not in self.config:
          self.config['Authentication']="munge"
      self.auth=auth.authentication(self.config)

      for system in self.config['Platforms']:
          self.systems.append(system)
      # Connect to database
      if 'MongoDBURI' in self.config:
          client = MongoClient(self.config['MongoDBURI'])
          db=self.config['MongoDB']
          self.images=client[db].images
      else:
          raise NameError('MongoDBURI not defined')
      # Initialize data structures

  def check_session(self,session,system=None):
      """Check if this is a valid session
      session is a session handle
      """
      if 'magic' not in session:
          return False
      elif session['magic'] is not self.magic:
          return False
      if system is not None and session['system'] is not system:
          return False
      return True

  def isasystem(self,system):
      """Check if system is a valid platform."""
      if system in self.systems:
          return True
      else:
          return False

  def checkread(self,user,imageid):
      """Checks if the user has read permissions to the image. (Not Implemented)"""
      return True

  def resetexpire(self,imageid):
      """Reset the expire time.  (Not Implemented)."""
      # Change expire time for image
      return True

  def new_session(self,authString,system):
      """
      Creates a session context that can be used for multiple
      Transactions.
      auth is an auth string that will be passed to the authenication
      layer.
      Returns a context that can be used for subsequent operations.
      """
      arec=self.auth.authenticate(authString,system)
      if arec is None:
          raise OSError("Authenication returned None")
      else:
          session=dict()
          if len(arec)==3:
              (uid,gid,token)=arec
          elif len(arec)==2:
              (uid,gid)=arec
              token=''
          else:
              raise OSError("Authentication returned invalid response")
          session['magic']=self.magic
          session['system']=system
          session['uid']=uid
          session['gid']=gid
          session['token']=token
          return session

  def lookup(self,session,image):
      """
      Lookup an image.
      Image is dictionary with system,itype and tag defined.
      """
      if not self.check_session(session,image['system']):
          raise OSError("Invalid Session")
      q={'system':image['system'],
        'itype':image['itype'],
        'tag':image['tag']}
      self.update_states()
      rec=self.images.find_one(q)
      # verify access
      return rec


  def list(self,session,system):
        """
        list images for a system.
        Image is dictionary with system defined.
        """
        if not self.check_session(session,system):
            raise OSError("Invalid Session")
        q={'system':system}
        self.update_states()
        records=self.images.find(q)
        resp=[]
        for record in records:
            resp.append(record)
        #print "list: records "+str(rec[0:100])
        # verify access
        return resp



  def isready(self,image):
      """
      Helper function to determine if an image is READY.
      """
      q={'system':image['system'],'itype':image['itype'],'tag':image['tag']}
      rec=self.images.find_one(q)
      if rec is not None and 'status' in rec and rec['status']=='READY':
          return True
      return False

  def refreshable(self,image):
      """
      Is the last pull old enough that we can re-pull
      """
      q={'system':image['system'],'itype':image['itype'],'tag':image['tag']}
      rec=self.images.find_one(q)
      now=time()
      if rec['status']!='READY':
          return True
      if 'last_pull' not in rec:
          return True
      if (now>(self.config['PullUpdateTimeout']+rec['last_pull'])):
          return True
      return False


  def new_image_record(self,session,image):
      """
      Creates a new image in mongo.  If the image already exist it returns the mongo record.
      """
      record=self.lookup(session,image)
      if record is not None:
          return record
      newimage={'format':'ext4',#<ext4|squashfs|vfs>
          'arch':'amd64', #<amd64|...>
          'os':'linux', #<linux|...>
          'location':'', #<urlencoded location, interpretation dependent on remotetype>
          'remotetype':'dockerv2', #<file|dockerv2|amazonec2>
          'ostcount':'0', #<integer, number of OSTs to per-copy of the image>
          'replication':'1', #<integer, number of copies to deploy>
          'userAcl':[],
          'status':'UNKNOWN',
          'groupAcl':[]}
      for p in image:
          newimage[p]=image[p]
      self.images.insert(newimage)
      return newimage


  def pull(self,session,request,delay=True,TESTMODE=0):
      """
      pull the image
      Takes an auth token, a request object
      Optional: deplay=True/False
      """
      self.logger.debug('Pull called Test Mode=%d'%TESTMODE) # will print a message to the console
      if not self.check_session(session,request['system']):
          raise OSError("Invalid Session")
      if self.isready(request)==False or self.refreshable(request):
          rec=self.new_image_record(session,request)
          id=rec.pop('_id',None)
          self.logger.debug("Setting state")
          self.update_mongo_state(id,'ENQUEUED')
          if delay:
              self.logger.debug("Calling do pull with queue=%s"%(request['system']))
              pullreq=dopull.apply_async([request],queue=request['system'],
                    kwargs={'TESTMODE':TESTMODE})
          else: # Fake celery
              dopull(request)
              pullreq=id
          self.update_mongo(id,{'last_pull':time()})
          self.task_image_id[pullreq]=id
          self.tasks.append(pullreq)
          #def pullImage(options, config[''], repo, tag, cacert=None, username=None, password=None):
          #pullImage(None, 'https://registry.services.nersc.gov', 'lcls_xfel_edison', '201509081609', cacert='local.crt')
      else:
        rec=self.new_image_record(session,request)
        id=rec.pop('_id',None)
      return id

  def update_mongo_state(self,id,state):
      """
      Helper function to set the mongo state for an image with _id==id to state=state.
      """
      if state=='SUCCESS':
          state='READY'
      self.images.update({'_id':id},{'$set':{'status':state}})

  def update_mongo(self,id,resp):
      """
      Helper function to set the mongo values for an image with _id==id.
      """
      setline=dict()
      if 'id' in resp:
          setline['id']=resp['id']
      if 'entrypoint' in resp:
          setline['ENTRY']=resp['entrypoint']
      if 'env' in resp:
          setline['ENV']=resp['env']
      if 'workdir' in resp:
          setline['WORKDIR']=resp['workdir']
      if 'last_pull' in resp:
          setline['last_pull']=resp['last_pull']

      self.images.update({'_id':id},{'$set':setline})


  def get_state(self,id):
      """
      Lookup the state of the image with _id==id in Mongo.  Returns the state."""
      self.update_states()
      return self.images.find_one({'_id':id},{'status':1})['status']


  def update_states(self):
      """
      Update the states of all active transactions.
      """
      #logger.debug("Update_states called")
      i=0
      tasks=self.tasks
      for req in tasks:
          state='PENDING'

          if isinstance(req,celery.result.AsyncResult):
              state=req.state
          elif isinstance(req,bson.objectid.ObjectId):
              print "Non-Async"
          #print self.task_image_id[req]
          self.update_mongo_state(self.task_image_id[req],state)
          if state=="READY" or state=="SUCCESS":
              self.logger.debug("Cleaning up request %d"%i)
              response=req.get()
              self.update_mongo(self.task_image_id[req],response)
              self.logger.debug('meta=%s'%(str(response)))
              # Now save the response
              self.tasks.remove(req)
          i+=1

  def expire(self,session,system,type,tag,id):
      """Expire an image.  (Not Implemented)"""
      # Do lookup
      resp=dict()
      return resp

def usage():
    """Print usage"""
    print "Usage: imagemngr <lookup|pull|expire>"
    sys.exit(0)

if __name__ == '__main__':
    configfile='test.json'
    if 'CONFIG' in os.environ:
        configfile=os.environ['CONFIG']
    m=imagemngr(configfile)
    sys.argv.pop(0)
    if len(sys.argv)<1:
        usage()
        sys.exit(0)
    command=sys.argv.pop(0)
    if command=='lookup':
        if len(sys.argv)<3:
            usage()
    elif command=='list':
        if len(sys.argv)<1:
            usage()
    elif command=='pull':
        if len(sys.argv)<3:
            usage()
        print "pull"
        req=dict()
        (req['system'],req['itype'],req['tag'])=sys.argv
        m.pull('good',req)
    else:
        print "Unknown command %s"%(command)
        usage()
