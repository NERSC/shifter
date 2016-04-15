#!/usr/bin/python

from pymongo import MongoClient
import json
from imageworker import dopull,initqueue
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

  def __init__(self, config, logname='imagemngr'):
      """
      Create an instance of the image manager.
      """
      logformat='%(asctime)s %(levelname)s: %(message)s [in %(pathname)s:%(lineno)d]'
      self.logger = logging.getLogger(logname)
      ch = logging.StreamHandler()
      formatter = logging.Formatter('%(asctime)s [%(name)s] %(levelname)s : %(message)s')
      ch.setFormatter(formatter)
      ch.setLevel(logging.DEBUG)
      self.logger.addHandler(ch)

      self.logger.debug('Initializing image manager')
      self.config=config
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
      initqueue(config)
      # Initialize data structures

  def check_session(self,session,system=None):
      """Check if this is a valid session
      session is a session handle
      """
      if 'magic' not in session:
          self.logger.warn("no magic")
          return False
      elif session['magic'] is not self.magic:
          self.logger.warn("bad magic %s"%(session['magic']))
          return False
      if system is not None and session['system']!=system:
          self.logger.warn("bad system %s!=%s"%(session['system'],system))
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

  def resetexpire(self,id):
      """Reset the expire time.  (Not Implemented)."""
      # Change expire time for image
      (days,hours,minutes,secs)=self.config['ImageExpirationTimeout'].split(':')
      expire=time()+int(secs)+60*(int(minutes)+60*(int(hours)+24*int(days)))
      self.images.update({'_id':id},{'$set':{'expiration':expire}})
      return expire

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
      q={'status':'READY',
        'system':image['system'],
        'itype':image['itype'],
        'tag':{'$in':[image['tag']]}}
      self.update_states()
      rec=self.images.find_one(q)
      if rec is not None:
          self.resetexpire(rec['_id'])
      # TODO: verify access
      return rec


  def list(self,session,system):
        """
        list images for a system.
        Image is dictionary with system defined.
        """
        if not self.check_session(session,system):
            raise OSError("Invalid Session")
        q={'status':'READY','system':system}
        self.update_states()
        records=self.images.find(q)
        resp=[]
        for record in records:
            resp.append(record)
        # verify access
        return resp



  def isready(self,image):
      """
      Helper function to determine if an image is READY.
      """
      q={'status':'READY','system':image['system'],'itype':image['itype'],'tag':{'$in':[image['tag']]}}
      rec=self.images.find_one(q)
      if rec is not None:
          return True
      return False


  def pullable(self,rec):
    """
    An image is pullable when:
    -There is no existing record
    -The status is a FAILURE
    -The status is READY and it is past the update time
    -The state is something else and the pull has expired
    """

    # if rec is None then do a pull
    if rec is None:
        return True

    # Okay there has been a pull before
    # If the status flag is missing just repull (shouldn't happen)
    if 'status' not in rec:
        return True
    status=rec['status']

    # Need to deal with last_pull for a READY record
    if 'last_pull' not in rec:
        return True
    nextpull=self.config['PullUpdateTimeout']+rec['last_pull']
    # It has been a while, so re-pull to see if it is fresh
    if status=='READY' and (time()>nextpull):
        return True

    # Repull failed pulls
    if status=='FAILURE' and (time()>nextpull):
        return True
    # Last thing... What if the pull somehow got hung or died in the middle
    # TODO: add pull timeout.  For now use the same one
    if time()>nextpull:
        return True
    return False


  def new_pull_record(self,session,image):
      """
      Creates a new image in mongo.  If the pull already exist it removes it first.
      """
      # Clean out any existing records
      for rec in self.images.find(image):
          if rec['status']=='READY':
              continue
          else:
              self.images.remove({'_id':rec['_id']})
      newimage={'format':'ext4',#<ext4|squashfs|vfs>
          'arch':'amd64', #<amd64|...>
          'os':'linux', #<linux|...>
          'location':'', #<urlencoded location, interpretation dependent on remotetype>
          'remotetype':'dockerv2', #<file|dockerv2|amazonec2>
          'ostcount':'0', #<integer, number of OSTs to per-copy of the image>
          'replication':'1', #<integer, number of copies to deploy>
          'userAcl':[],
          'tag':[],
          'status':'INIT',
          'groupAcl':[]}
      for p in image:
          if p is 'tag':
              continue
          newimage[p]=image[p]
      self.images.insert(newimage)
      return newimage


  def pull(self,session,image,TESTMODE=0):
      """
      pull the image
      Takes an auth token, a request object
      Optional: TESTMODE={0,1,2} See below...
      """
      request={'system':image['system'],
            'itype':image['itype'],
            'pulltag':image['tag']}
      self.logger.debug('Pull called Test Mode=%d'%TESTMODE)
      if not self.check_session(session,request['system']):
          self.logger.warn('Invalid session on system %s'%{request['system']})
          raise OSError("Invalid Session")
      # If a pull request exist for this tag
      #  check to see if it is expired or a failure, if so remove it
      # otherwise
      #  return the record
      rec=None
      # find any pull record
      self.update_states()
      # let's lookup the active image
      q={'status':'READY',
          'system':image['system'],
          'itype':image['itype'],
          'tag':{'$in':[image['tag']]}}
      rec=self.images.find_one(q)
      for r in self.images.find(request):
          st=r['status']
          if st=='READY' or st=='SUCCESS':
              continue
          rec=r
          break


      if self.pullable(rec):
          rec=self.new_pull_record(session,request)
          id=rec['_id']
          self.logger.debug("Setting state")
          self.update_mongo_state(id,'ENQUEUED')
          request['tag']=request['pulltag']
          self.logger.debug("Calling do pull with queue=%s"%(request['system']))
          pullreq=dopull.apply_async([request],queue=request['system'],
                    kwargs={'TESTMODE':TESTMODE})
          self.logger.info("pull request queued s=%s t=%s"%(request['system'],request['tag']))
          self.update_mongo(id,{'last_pull':time()})
          self.task_image_id[pullreq]=id
          self.tasks.append(pullreq)
          #def pullImage(options, config[''], repo, tag, cacert=None, username=None, password=None):
          #pullImage(None, 'https://registry.services.nersc.gov', 'lcls_xfel_edison', '201509081609', cacert='local.crt')

      return rec

  def update_mongo_state(self,id,state):
      """
      Helper function to set the mongo state for an image with _id==id to state=state.
      """
      if state=='SUCCESS':
          state='READY'
      self.images.update({'_id':id},{'$set':{'status':state}})

  def add_tag(self,id,system,tag):
      """
      Helper function to add a tag to an image. id is the mongo id (not image id)
      """
      #self.images.update({'_id':id},{'$set':})
      # Remove the tag first
      self.remove_tag(system,tag)
      # see if tag isn't a list
      rec=self.images.find_one({'_id':id})
      if rec is not None and 'tag' in rec and not isinstance(rec['tag'],(list)):
          self.logger.info('Fixing tag for non-list %s %s'%(id,str(rec['tag'])))
          curtag=rec['tag']
          self.images.update({'_id':id},{'$set':{'tag':[curtag]}})
      self.images.update({'_id':id},{'$addToSet':{'tag':tag}})
      return True

  def remove_tag(self,system,tag):
      """
      Helper function to remove a tag to an image.
      """
      self.images.update({ 'system':system,'tag': { '$in':[tag]}},{'$pull':{'tag':tag}},multi=True)
      # for old tag format
      for rec in self.images.find({'system':system,'tag':tag }):
          if isinstance(rec['tag'],(str)):
              self.images.update({'_id':id},{'$set':{'tag':[]}})
      #This didn't work
      #self.images.update({ '$and':[ {'tag': { '$type' : 2}},{'tag':tag }]},{'$set':{'tag':[]}},multi=True)

      return True

  def complete_pull(self,id,response):
      """
      Transition a completed pull request to an available image.
      """

      self.logger.info("Complete called for %s %s"%(id,str(response)))
      pullrec=self.images.find_one({'_id':id})
      if pullrec is None:
          self.logger.warn('Missing pull request (r=%s)'%(str(response)))
          return
      #Check that this image id doesn't already exist for this system
      rec=self.images.find_one({'id':response['id'], 'system': pullrec['system']})
      tag=pullrec['pulltag']
      if rec is not None:
          # So we already had this image.
          # Let's delete the pull record.
          # TODO: update the pull time of the matching id
          self.update_mongo(rec['_id'],{'last_pull':time()})

          self.images.remove({'_id':id})
          # However it could be a new tag.  So let's update the tag
          try:
              index=rec['tag'].index(response['tag'])
          except:
              self.add_tag(rec['_id'],pullrec['system'],tag)
          return True
      else:
          response['last_pull']=time()
          self.update_mongo(id,response)
          self.add_tag(id,pullrec['system'],tag)


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
      Cleanup failed transcations after a period
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
          self.update_mongo_state(self.task_image_id[req],state)
          if state=="READY" or state=="SUCCESS":
              self.logger.debug("Completing request %d"%i)
              response=req.get()
              self.complete_pull(self.task_image_id[req],response)
              self.logger.debug('meta=%s'%(str(response)))
              # Now save the response
              self.tasks.remove(req)
          if state=="FAILURE":
              self.logger.warn("Pull failed for %s"%(req))
          i+=1
      # Look for failed pulls
      for rec in self.images.find({'status':'FAILURE'}):
          nextpull=self.config['PullUpdateTimeout']+rec['last_pull']
          # It it has been a while then let's clean up
          if (time()>nextpull):
              self.images.remove({'_id':rec['_id']})


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
    with open(configfile) as cf:
        config = json.load(cf)
    m=imagemngr(config)
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
        req=dict()
        (req['system'],req['itype'],req['tag'])=sys.argv
        m.pull('good',req)
    else:
        print "Unknown command %s"%(command)
        usage()
