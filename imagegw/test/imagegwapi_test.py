import os
import unittest
import time
import urllib
import json
from pymongo import MongoClient

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

AUTH_HEADER = 'authentication'


class GWTestCase(unittest.TestCase):

    def setUp(self):
        self.configfile = 'test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        os.environ['GWCONFIG'] = self.configfile

        from shifter_imagegw import api
        mongouri = self.config['MongoDBURI']
        print "Debug: Connecting to %s" % mongouri
        client = MongoClient(mongouri)
        db = self.config['MongoDB']
        if not os.path.exists(self.config['CacheDirectory']):
            os.mkdir(self.config['CacheDirectory'])
        p = self.config['Platforms']['systema']['ssh']['imageDir']
        if not os.path.exists(p):
            os.makedirs(p)
        self.images = client[db].images
        self.images.drop()
        self.metrics = client[db].metrics
        self.metrics.remove({})
        api.config['TESTING'] = True
        self.app = api.app.test_client()
        self.url = "/api"
        self.system = "systema"
        self.type = "docker"
        self.itag = "ubuntu:14.04"
        self.tag = urllib.quote(self.itag)
        self.urlreq = "%s/%s/%s" % (self.system, self.type, self.tag)
        # Need to switch to real munge tokens
        self.auth = "good:user:user::500:500"
        self.authadmin = 'good:root:root::0:0'
        self.auth_bad = "bad:user:user::501:501"
        self.auth_header = 'authentication'
        self.logfile = '/tmp/worker.log'
        self.pid = 0
        #if os.path.exists(self.logfile):
        #    os.unlink(self.logfile)
        self.start_worker()

    def tearDown(self):
        self.stop_worker()

    def start_worker(self, testmode=1, system='systema'):
        # Start a celery worker.
        pid = os.fork()
        if pid == 0:  # Child process
            os.environ['CONFIG'] = 'test.json'
            os.environ['TESTMODE'] = '%d' % (testmode)
            os.execvp('celery', ['celery', '-A', 'shifter_imagegw.imageworker',
                                 'worker', '--quiet', '-Q', '%s' % (system),
                                 '--loglevel=INFO', '-c', '1',
                                 '-f', self.logfile])
        else:
            self.pid = pid

    def stop_worker(self):
        print "Stopping worker"
        if self.pid > 0:
            os.kill(self.pid, 9)

    def time_wait(self, urlreq, state='READY', TIMEOUT=30):
        poll_interval = 0.5
        count = TIMEOUT / poll_interval
        cstate = 'UNKNOWN'
        uri = '%s/lookup/%s/' % (self.url, urlreq)
        while (cstate != state and count > 0):
            rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
            if rv.status_code != 200:
                time.sleep(1)
                continue
            r = json.loads(rv.data)
            cstate = r['status']
            if r['status'] == 'READY':
                break
            if r['status'] == 'FAILURE':
                break
            print '  %s...' % (r['status'])
            time.sleep(1)
            count = count - 1
        return cstate

    def good_record(self):
        return {'system': self.system,
                'itype': self.type,
                'id': 'bogus',
                'tag': [self.itag],
                'format': 'squashfs',
                'status': 'READY',
                'userACL': [],
                'groupACL': [],
                'last_pull': time.time(),
                'ENV': [],
                'ENTRY': '',
                }

    def test_pull(self):
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        data = {'allowed_uids': '1000,1001',
                'allowed_gids': '1002,1003'}
        datajson = json.dumps(data)
        rv = self.app.post(uri, headers={AUTH_HEADER: self.auth},
                           data=datajson)
        data = json.loads(rv.data)
        assert 'userACL' in data
        assert 'groupACL' in data
        #assert (data['userACL'] == [1000, 1001])
        assert 1000 in data['userACL']
        assert 500 in data['userACL']
        assert 500 in data['groupACL']
        assert 1002 in data['groupACL']
        assert rv.status_code == 200

    def test_list(self):
        # Do a pull so we can create an image record
        uri = '%s/list/%s/' % (self.url, 'systemc')
        rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        self.assertEquals(rv.status_code, 404)
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        rv = self.app.post(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status_code == 200
        uri = '%s/list/%s/' % (self.url, self.system)
        rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status_code == 200

    def test_queue(self):
        # Do a pull so we can create an image record
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        rv = self.app.post(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status_code == 200
        uri = '%s/queue/%s/' % (self.url, self.system)
        rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        print rv.data
        assert rv.status_code == 200

    def test_pulllookup(self):
        # Do a pull so we can create an image record
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        i = 0
        while i < 200:
            rv = self.app.post(uri, headers={AUTH_HEADER: self.auth})
            assert rv.status_code == 200
            r = json.loads(rv.data)
            if r['status'] == 'READY':
                break
            if r['status'] == 'FAILURE':
                break
            print '  %s %s...' % (r['status'], r['status_message'])
            time.sleep(1)
            i = i + 1
        print ''
        uri = '%s/lookup/%s/' % (self.url, self.urlreq)
        rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status_code == 200
        uri = '%s/lookup/%s/%s/' % (self.url, self.system, 'bogus')
        rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status_code == 404

    def test_lookup(self):
        # Do a pull so we can create an image record
        record = self.good_record()
        id = self.images.insert(record)
        assert id is not None
        uri = '%s/lookup/%s/' % (self.url, self.urlreq)
        rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status_code == 200

    def test_expire(self):
        uri = '%s/expire/%s/%s/%s/' % (self.url, self.system, self.type,
                                       self.tag)
        rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status_code == 200

    def test_autoexpire(self):
        record = self.good_record()
        record['expiration'] = time.time() - 100
        id = self.images.insert(record)
        uri = '%s/autoexpire/%s/' % (self.url, self.system)
        rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
        assert rv.status_code == 200
        assert rv.data.count('bogus') > 0

        count = 20
        while count > 0:
            rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
            r = self.images.find_one({'_id': id})
            print '   %s...' % (r['status'])
            if r['status'] == 'EXPIRED':
                break
            time.sleep(1)
            count = count - 1
        #self.time_wait(self.urlreq,state='EXPIRE')
        # Run again to trigger db update
        rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
        assert rv.status_code == 200
        r = self.images.find_one({'_id': id})

        assert r['status'] == 'EXPIRED'

    def test_metrics(self):
        rec = {
            "uid": 100,
            "user": "usera",
            "tag": self.tag,
            "system": self.system,
            "id": "fakeid",
            "type": "docker"
        }
        # Remove everything
        self.metrics.remove({})
        for _ in xrange(100):
            rec['time'] = time.time()
            last_time = rec['time']
            self.metrics.insert(rec.copy())
        uri = '%s/metrics/%s/?limit=20' % (self.url, self.system)
        rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
        self.assertEquals(rv.status_code, 200)
        data = json.loads(rv.data)
        self.assertEquals(len(data), 20)
        self.assertEquals(data[19]['time'], last_time)

if __name__ == '__main__':
    unittest.main()
