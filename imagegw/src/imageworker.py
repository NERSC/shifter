from celery import Celery
import json
import os
import time
import dockerv2
from pymongo import MongoClient
from bson.objectid import ObjectId

CONFIGFILE='imagemanager.json'
TESTMODE=0
DEBUG=0

if 'TESTMODE' in os.environ and os.environ['TESTMODE']=='1':
    print "Enabling Testmode"
    TESTMODE=1
if 'CONFIG' in os.environ:
    CONFIGFILE=os.environ['CONFIG']

if DEBUG:
  print "Opening %s"%(CONFIGFILE)

with open(CONFIGFILE) as configfile:
    config=json.load(configfile)

# Create Celery Queue and configure serializer
#
queue = Celery('tasks', backend='rpc://',broker=config['Broker'])
queue.conf.update(CELERY_ACCEPT_CONTENT = ['json'])
queue.conf.update(CELERY_TASK_SERIALIZER = 'json')
queue.conf.update(CELERY_RESULT_SERIALIZER = 'json')

#status: 	"uptodate", "enqueued", "transferFromRemote", "examination",
#"formatConversion", "transferToPlatform", "error"

@queue.task(bind=True)
def dopull(self,i):
    print "do pull %s"%(i['tag'])
    self.update_state(state='PULLING')
    if TESTMODE==1:
        time.sleep(1)
        for state in ('EXAMINATION','CONVERSION','TRANSFER','READY'):
            print "Worker: TESTMODE Updating to %s"%(state)
            self.update_state(state=state)
            time.sleep(1)
            return
    # Step 1 - Do the pull
    # Step 2 - Check the image
    # Step 3 - Convert
    # Step 4 - TRANSFER
    # Done
    self.update_state(state='READY')
    #thing = db.things.find_one({'_id': ObjectId('4ea113d6b684853c8e000001') })
