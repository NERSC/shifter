import os
import unittest
import time
import urllib.request, urllib.parse, urllib.error
import json
from copy import deepcopy
from shifter_imagegw.fasthash import fast_hash
from shifter_imagegw.imagemngr import ImageMngr
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
DEBUG = False


class GWTestCase(unittest.TestCase):
    def setUp(self):
        mongouri = self.config['MongoDBURI']
        # print "Debug: Connecting to %s" % mongouri
        client = MongoClient(mongouri)
        db = self.config['MongoDB']
        if not os.path.exists(self.config['CacheDirectory']):
            os.mkdir(self.config['CacheDirectory'])
        p = self.config['Platforms']['systema']['ssh']['imageDir']
        if not os.path.exists(p):
            os.makedirs(p)
        self.images = client[db].images
        self.images.drop()
        self.requests = client[db].requests
        self.requests.remove()
        self.metrics = client[db].metrics
        self.metrics.remove({})
        self.url = "/api"
        self.system = "systema"
        self.type = "docker"
        self.itag = "alpine:latest"
        self.tag = urllib.parse.quote(self.itag)
        self.urlreq = "%s/%s/%s" % (self.system, self.type, self.tag)
        # Need to switch to real munge tokens
        self.auth = "good:user:user::500:500"
        self.authadmin = 'good:root:root::0:0'
        self.auth_bad = "bad:user:user::501:501"
        self.auth_header = 'authentication'
        self.logfile = '/tmp/worker.log'
        self.pid = 0
        test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
        self.test_dir = test_dir

    @classmethod
    def setUpClass(cls):
        cls.configfile = 'test.json'
        with open(cls.configfile) as config_file:
            cls.config = json.load(config_file)
        os.environ['GWCONFIG'] = cls.configfile
        from shifter_imagegw import api
        api.config['TESTING'] = True
        cls.mgr = api.getmgr()
        cls.app = api.app.test_client

    @classmethod
    def tearDownClass(cls):
        cls.mgr.shutdown()

    def cleanup_pulls(self):
        path = self.config['Platforms']['systema']['ssh']['imageDir']
        for f in os.listdir(path):
            if f.endswith('.meta') or f.endswith('.squashfs'):
                os.remove(os.path.join(path, f))

    def time_wait(self, urlreq, data=None, state='READY', op='pull',
                  TIMEOUT=30):
        poll_interval = 0.5
        count = TIMEOUT / poll_interval
        cstate = 'UNKNOWN'
        uri = '%s/%s/%s/' % (self.url, op, urlreq)
        while (cstate != state and count > 0):
            _, rv = self.app.post(uri, data=data,
                               headers={AUTH_HEADER: self.auth})
            if rv.status != 200:
                time.sleep(1)
                continue
            r = rv.json
            cstate = r['status']
            if r['status'] == 'READY':
                break
            if r['status'] == 'FAILURE':
                break
            if DEBUG:
                print('  %s...' % (r['status']))
            time.sleep(1)
            count = count - 1
        return rv

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

    def test_pull_acl_checks(self):
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        data = {'allowed_uids': '1000,1001',
                'allowed_gids': '1002,1003'}
        datajson = json.dumps(data)
        _, rv = self.app.post(uri, headers={AUTH_HEADER: self.auth},
                           data=datajson)
        data = rv.json
        assert 'userACL' in data
        assert 'groupACL' in data
        assert 1000 in data['userACL']
        assert 500 in data['userACL']
        assert 500 in data['groupACL']
        assert 1002 in data['groupACL']
        rv = self.time_wait(self.urlreq)
        assert rv.status == 200

    def test_list(self):
        # Do a pull so we can create an image record
        uri = '%s/list/%s/' % (self.url, 'systemc')
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        self.assertEqual(rv.status, 404)
        # uri = '%s/pull/%s/' % (self.url, self.urlreq)
        # rv = self.app.post(uri, headers={AUTH_HEADER: self.auth})
        rv = self.time_wait(self.urlreq)
        assert rv.status == 200
        uri = '%s/list/%s/' % (self.url, self.system)
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status == 200

    def test_queue(self):
        # Do a pull so we can create an image record
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        _, rv = self.app.post(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status == 200
        uri = '%s/queue/%s/' % (self.url, self.system)
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status == 200

    def test_pulllookup(self):
        self.cleanup_pulls()
        # Do a pull so we can create an image record
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        rv = self.time_wait(self.urlreq)
        print(rv.json)
        uri = '%s/lookup/%s/' % (self.url, self.urlreq)
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        self.assertEquals(rv.status, 200)
        uri = '%s/lookup/%s/%s/' % (self.url, self.system, 'bogus')
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        self.assertEquals(rv.status, 404)

    def test_lookup(self):
        # Do a pull so we can create an image record
        record = self.good_record()
        id = self.images.insert(record)
        assert id is not None
        uri = '%s/lookup/%s/' % (self.url, self.urlreq)
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status == 200

    def test_expire(self):
        uri = '%s/expire/%s/%s/%s/' % (self.url, self.system, self.type,
                                       self.tag)
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.auth})
        assert rv.status == 200

    def test_autoexpire(self):
        record = self.good_record()
        record['expiration'] = time.time() - 100
        id = self.images.insert(record)
        uri = '%s/autoexpire/%s/' % (self.url, self.system)
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
        assert rv.status == 200
        #assert rv.data.decode("utf-8").count('bogus') > 0

        count = 20
        while count > 0:
            _, rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
            r = self.images.find_one({'_id': id})
            if r['status'] == 'EXPIRED':
                break
            time.sleep(1)
            count = count - 1
        # self.time_wait(self.urlreq,state='EXPIRE')
        # Run again to trigger db update
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
        assert rv.status == 200
        r = self.images.find_one({'_id': id})

        self.assertEqual(r['status'], 'EXPIRED')

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
        for _ in range(100):
            rec['time'] = time.time()
            last_time = rec['time']
            self.metrics.insert(rec.copy())
        uri = '%s/metrics/%s/?limit=20' % (self.url, self.system)
        _, rv = self.app.get(uri, headers={AUTH_HEADER: self.authadmin})
        self.assertEqual(rv.status, 200)
        data = rv.json
        self.assertEqual(len(data), 20)
        self.assertEqual(data[19]['time'], last_time)

    def test_import(self):
        self.config["ImportUsers"] = "all"

        uri = '%s/doimport/%s/' % (self.url, self.urlreq)
        ifile = os.path.join(self.test_dir, 'test.squashfs')
        data = {'filepath': ifile,
                'format': 'squashfs'}
        hash = fast_hash(ifile)
        datajson = json.dumps(data)
        _, rv = self.app.post(uri, headers={AUTH_HEADER: self.auth},
                           data=datajson)
        rv = self.time_wait(self.urlreq, op='doimport', data=datajson)
        # for i in range(5):
        #     rv = self.app.post(uri, headers={AUTH_HEADER: self.auth},
        #                        data=datajson)
        #     data = json.loads(rv.data)
        #     if data['status'] == 'READY':
        #         break
        #     time.sleep(1)
        data = rv.json
        self.assertEqual(data['status'], 'READY')
        self.assertEqual(data['id'], hash)
#        assert 'filepath' in data
#        assert rv.status == 200

    def test_labels(self):
        # Configure an API service with use_external
        from shifter_imagegw import api
        c = deepcopy(self.config)
        c['Platforms']['systema']['use_external'] = True
        api.mgr = ImageMngr(c)
        # Do a pull so we can create an image record
        uri = '%s/pull/%s/' % (self.url, self.urlreq)
        app = api.app.test_client
        app.post(uri, headers={AUTH_HEADER: self.auth})
        time.sleep(1)
        _, rv = app.post(uri, headers={AUTH_HEADER: self.auth})
        self.assertEqual(rv.json['status'], 'READY')
        uri = '%s/lookup/%s/' % (self.url, self.urlreq)
        _, rv = app.get(uri, headers={AUTH_HEADER: self.auth})
        self.assertEquals(rv.status, 200)
        resp = rv.json
        self.assertIn('LABELS', resp)
        self.assertIn('alabel', resp['LABELS'])
        self.assertEqual(resp['LABELS']['alabel'], 'avalue')


if __name__ == '__main__':
    unittest.main()
