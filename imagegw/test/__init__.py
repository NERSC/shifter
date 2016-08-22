import os
import subprocess

def setup():
    print "Module setup"
    os.environ['GWCONFIG'] = 'test.json'
    os.environ['CONFIG'] = 'test.json'
    test_dir = os.path.dirname(os.path.abspath(__file__))
    os.environ['PATH'] = '%s:%s' % (test_dir, os.environ['PATH'])
    # Create __init__

def teardown():
    subprocess.check_call("cleanup.sh")
