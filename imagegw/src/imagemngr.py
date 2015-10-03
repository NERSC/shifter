#!/usr/bin/python

from pymongo import MongoClient
import json
from imageworker import dopull
import bson
import celery


# Image Manager class
# This class handles most of the backend work
class imagemngr:

  def __init__(self, CONFIGFILE):
      with open(CONFIGFILE) as config_file:
          self.config = json.load(config_file)
      if 'Platforms' not in self.config:
          raise NameError('Platforms not defined')
      self.systems=[]
      self.tasks=[]
      self.task_image_id=dict()
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


  # Check if this is a valid system
  def isasystem(self,system):
      if system in self.systems:
          return True
      else:
          return False

  # ACLs
  def checkread(self,user,imageid):
      return True

  # Reset the expire time
  def resetexpire(self,imageid):
      # Change expire time for image
      return True


  def lookup(self,auth,image):
      # Do lookup
      q={'system':image['system'],'itype':image['itype'],'tag':image['tag']}
      rec=self.images.find_one(q)
      # verify access
      return rec

  def isready(self,image):
      q={'system':image['system'],'itype':image['itype'],'tag':image['tag']}
      rec=self.images.find_one(q)
      if rec is not None and 'status' in rec and rec['status']=='READY':
          return True
      return False

  def image_record(self,image):
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
      return newimage


  def pull(self,auth,request,delay=True):
      if self.isready(request)==False:
          rec=self.image_record(request)
          rec['status']='ENQUEUED'
          result=self.images.insert(rec)
          id=rec.pop('_id',None)
          if result==None:
              return False
          if delay:
              pullreq=dopull.delay(request)
          else: # Fake celery
              dopull(request)
              pullreq=id
          self.task_image_id[pullreq]=id
          self.tasks.append(pullreq)
          #def pullImage(options, config[''], repo, tag, cacert=None, username=None, password=None):
          #pullImage(None, 'https://registry.services.nersc.gov', 'lcls_xfel_edison', '201509081609', cacert='local.crt')
      return id

  def update_mongo_state(self,id,state):
      if state=='SUCCESS':
          state='READY'
      self.images.update({'_id':id},{'$set':{'status':state}})

  def get_state(self,id):
      self.update_states()
      return self.images.find_one({'_id':id},{'status':1})['status']


  def update_states(self):
      for req in self.tasks:
          state='PENDING'
          if isinstance(req,celery.result.AsyncResult):
              state=req.state
          elif isinstance(req,bson.objectid.ObjectId):
              print "Non-Async"
          #print self.task_image_id[req]
          self.update_mongo_state(self.task_image_id[req],state)
          #self.images.update({'_id':self.task_image_id[req]},{'$set':{'status':req.state}})
          #print self.images.find_one()


  def expire(self,auth,system,type,tag,id):
      # Do lookup
      resp=dict()
      return resp
