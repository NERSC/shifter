import os
import subprocess

def setup():
  print "Module setup"
  os.environ['GWCONFIG']='test.json'
  os.environ['CONFIG']='test.json'
  os.environ['PATH']='%s/test:%s'%(os.curdir,os.environ['PATH'])
  # Create __init__

def teardown():
  subprocess.check_call("./test/cleanup.sh") 
