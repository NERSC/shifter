import os
import unittest
import time
import json
import base64
import logging
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


class ImageMngrTestCase(unittest.TestCase):

    def setUp(self):
        from shifter_imagegw.imagemngr import ImageMngr
        self.configfile = 'test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        mongodb = self.config['MongoDBURI']
        client = MongoClient(mongodb)
        db = self.config['MongoDB']
        self.images = client[db].images
        self.metrics = client[db].metrics
        self.images.drop()
        self.logger = logging.getLogger("imagemngr")
        if len(self.logger.handlers) < 1:
            print "Number of loggers %d" % (len(self.logger.handlers))
            log_handler = logging.FileHandler('testing.log')
            logfmt = '%(asctime)s [%(name)s] %(levelname)s : %(message)s'
            log_handler.setFormatter(logging.Formatter(logfmt))
            log_handler.setLevel(logging.INFO)
            self.logger.addHandler(log_handler)
        self.m = ImageMngr(self.config, logger=self.logger)
        self.system = 'systema'
        self.itype = 'docker'
        self.tag = 'alpine:latest'
        self.id = 'fakeid'
        self.tag2 = 'test2'
        self.public = 'index.docker.io/busybox:latest'
        self.private = 'index.docker.io/scanon/shaneprivate:latest'
        self.format = 'squashfs'
        self.auth = 'good:user:user::100:100'
        self.authadmin = 'good:root:root::0:0'
        self.badauth = 'bad:user:user::100:100'
        self.logfile = '/tmp/worker.log'
        self.pid = 0
        self.query = {'system': self.system, 'itype': self.itype,
                      'tag': self.tag}
        self.pull = {'system': self.system, 'itype': self.itype,
                     'tag': self.tag, 'remotetype': 'dockerv2',
                     'userACL': [], 'groupACL': []}
        if os.path.exists(self.logfile):
            pass  # os.unlink(self.logfile)
        # Cleanup Mongo
        self.images.remove({})

    def tearDown(self):
        """
        tear down should stop the worker
        """
        self.m.shutdown()

    def time_wait(self, id, wstate='READY', TIMEOUT=30):
        poll_interval = 0.5
        count = TIMEOUT / poll_interval
        state = 'UNKNOWN'
        while (state != wstate and count > 0):
            state = self.m.get_state(id)
            if state is None:
                return None
            count -= 1
            time.sleep(poll_interval)
        return state

    def get_metafile(self, system, id):
        if self.config['Platforms'][system]['accesstype'] == 'remote':
            idir = self.config['Platforms'][system]['ssh']['imageDir']
        else:
            idir = self.config['Platforms'][system]['local']['imageDir']

        metafile = os.path.join(idir, '%s.meta' % (id))
        return metafile

    def create_fakeimage(self, system, id, format):
        if self.config['Platforms'][system]['accesstype'] == 'remote':
            idir = self.config['Platforms'][system]['ssh']['imageDir']
        else:
            idir = self.config['Platforms'][system]['local']['imageDir']

        if os.path.exists(idir) is False:
            os.makedirs(idir)
        file = os.path.join(idir, '%s.%s' % (id, format))
        with open(file, 'w') as f:
            f.write('')
        metafile = os.path.join(idir, '%s.meta' % (id))
        with open(metafile, 'w') as f:
            f.write('')
        return file, metafile

    def good_pullrecord(self):
        return {'system': self.system,
                'itype': self.itype,
                'id': self.id,
                'pulltag': self.tag,
                'status': 'READY',
                'userACL': [],
                'groupACL': [],
                'ENV': [],
                'ENTRY': '',
                'last_pull': time.time()
                }

    def good_record(self):
        return {
            'system': self.system,
            'itype': self.itype,
            'id': self.id,
            'tag': [self.tag],
            'format': self.format,
            'status': 'READY',
            'userACL': [],
            'groupACL': [],
            'last_pull': time.time(),
            'ENV': [],
            'ENTRY': ''
        }

    def read_metafile(self, metafile):
        kv = {}
        with open(metafile) as mf:
            for line in mf:
                (k, v) = line.rstrip().split(': ')
                kv[k] = v
        # Convert ACLs to list of ints
        if 'USERACL' in kv:
            list = map(lambda x: int(x), kv['USERACL'].split(','))
            kv['USERACL'] = list
        if 'GROUPACL' in kv:
            list = map(lambda x: int(x), kv['GROUPACL'].split(','))
            kv['GROUPACL'] = list

        return kv

    def set_last_pull(self, id, t):
        self.m.images.update({'_id': id}, {'$set': {'last_pull': t}})

    def read_tokens(self):
        if not os.path.exists("./tokens.cfg"):
            return None
        with open("./tokens.cfg") as f:
            d = json.load(f)
        for k in d.keys():
            d[k] = base64.b64decode(d[k])
        return d

#
#  Tests
#
    def test_session(self):
        s = self.m.new_session(self.auth, self.system)
        assert s is not None
        try:
            s = self.m.new_session(self.badauth, self.system)
        except:
            pass

    def test_noadmin(self):
        s = self.m.new_session(self.auth, self.system)
        assert s is not None
        resp = self.m._isadmin(s, self.system)
        assert resp is False

    def test_admin(self):
        s = self.m.new_session(self.authadmin, self.system)
        assert s is not None
        resp = self.m._isadmin(s, self.system)
        assert resp is True


# Create an image and tag with a new tag.
# Make sure both tags show up.
# Remove the tag and make sure the original tags
# doesn't also get removed.
    def test_0add_remove_tag(self):
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)

        assert id is not None
        before = self.images.find_one({'_id': id})
        assert before is not None
        # Add a tag a make sure it worked
        status = self.m.add_tag(id, self.system, 'testtag')
        assert status is True
        after = self.images.find_one({'_id': id})
        assert after is not None
        assert after['tag'].count('testtag') == 1
        assert after['tag'].count(self.tag) == 1
        # Remove a tag and make sure it worked
        status = self.m.remove_tag(self.system, 'testtag')
        assert status is True
        after = self.images.find_one({'_id': id})
        assert after is not None
        assert after['tag'].count('testtag') == 0

    # Similar to above but just test the adding part
    def test_0add_remove_tagitem(self):
        record = self.good_record()
        record['tag'] = self.tag
        # Create a fake record in mongo
        id = self.images.insert(record)

        status = self.m.add_tag(id, self.system, 'testtag')
        assert status is True
        rec = self.images.find_one({'_id': id})
        assert rec is not None
        assert rec['tag'].count(self.tag) == 1
        assert rec['tag'].count('testtag') == 1

    # Same as above but use the lookup instead of a directory
    # direct mongo lookup
    def test_0add_remove_withtag(self):
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)

        session = self.m.new_session(self.auth, self.system)
        i = self.query.copy()
        status = self.m.add_tag(id, self.system, 'testtag')
        assert status is True
        rec = self.m.lookup(session, i)
        assert rec is not None
        assert rec['tag'].count(self.tag) == 1
        assert rec['tag'].count('testtag') == 1

    # Test if tag isn't a list
    def test_0add_remove_two(self):
        record = self.good_record()
        # Create a fake record in mongo
        id1 = self.images.insert(record.copy())
        record['id'] = 'fakeid2'
        record['tag'] = []
        id2 = self.images.insert(record.copy())

        status = self.m.add_tag(id2, self.system, self.tag)
        assert status is True
        rec1 = self.images.find_one({'_id': id1})
        rec2 = self.images.find_one({'_id': id2})
        assert rec1['tag'].count(self.tag) == 0
        assert rec2['tag'].count(self.tag) == 1

    # Similar to above but just test the adding part
    def test_0add_same_image_two_system(self):
        record = self.good_record()
        record['tag'] = self.tag
        # Create a fake record in mongo
        id1 = self.images.insert(record.copy())
        # add testtag for systema
        status = self.m.add_tag(id1, self.system, 'testtag')
        assert status is True
        record['system'] = 'systemb'
        id2 = self.images.insert(record.copy())
        status = self.m.add_tag(id2, 'systemb', 'testtag')
        assert status is True
        # Now make sure testtag for first system is still
        # present
        rec = self.images.find_one({'_id': id1})
        assert rec is not None
        assert rec['tag'].count('testtag') == 1

    def test_0isasystem(self):
        assert self.m._isasystem(self.system) is True
        assert self.m._isasystem('bogus') is False

    def test_0resetexp(self):
        record = {'system': self.system,
                  'itype': self.itype,
                  'id': self.id,
                  'pulltag': self.tag,
                  'status': 'READY',
                  'userACL': [],
                  'groupACL': [],
                  'ENV': [],
                  'ENTRY': '',
                  'last_pull': 0
                  }
        id = self.images.insert(record.copy())
        assert id is not None
        expire = self.m._resetexpire(id)
        assert expire > time.time()
        rec = self.images.find_one({'_id': id})
        assert rec['expiration'] == expire

    def test_0pullable(self):
        # An old READY image
        rec = {'last_pull': 0, 'status': 'READY'}
        assert self.m._pullable(rec) is True
        rec = {'last_pull': time.time(), 'status': 'READY'}
        # A recent READY image
        assert self.m._pullable(rec) is False

        rec = {'last_pull': time.time(),
               'last_heartbeat': 0,
               'status': 'READY'}
        # A recent READY image but an old heartbeat (maybe re-pulled)
        assert self.m._pullable(rec) is False

        # A failed image
        rec = {'last_pull': 0, 'status': 'FAILURE'}
        assert self.m._pullable(rec) is True
        # recent pull
        rec = {'last_pull': time.time(), 'status': 'FAILURE'}
        assert self.m._pullable(rec) is False

        # A hung pull
        rec = {'last_pull': 0, 'last_heartbeat': time.time() - 7200,
               'status': 'PULLING'}
        assert self.m._pullable(rec) is True
        # recent pull
        rec = {'last_pull': time.time(), 'status': 'PULLING'}
        assert self.m._pullable(rec) is False

        # A hung pull
        rec = {'last_pull': 0, 'last_heartbeat': time.time(),
               'status': 'PULLING'}
        assert self.m._pullable(rec) is False

    def test_0complete_pull(self):
        # Test complete_pull
        record = {'system': self.system,
                  'itype': self.itype,
                  'id': self.id,
                  'pulltag': self.tag,
                  'status': 'READY',
                  'userACL': [],
                  'groupACL': [],
                  'ENV': [],
                  'ENTRY': '',
                  'last_pull': 0
                  }
        record = self.good_pullrecord()
        record['last_pull'] = 0
        # Create a fake record in mongo
        # First test when there is no existing image
        id = self.images.insert(record.copy())
        assert id is not None
        resp = {'id': id, 'tag': self.tag}
        self.m.complete_pull(id, resp)
        rec = self.images.find_one({'_id': id})
        assert rec is not None
        assert rec['tag'] == [self.tag]
        assert rec['last_pull'] > 0
        # Create an identical request and
        # run complete again
        id2 = self.images.insert(record.copy())
        assert id2 is not None
        self.m.complete_pull(id2, resp)
        # confirm that the record was removed
        rec2 = self.images.find_one({'_id': id2})
        assert rec2 is None

    def test_0update_states(self):
        # Test a repull
        record = self.good_record()
        record['last_pull'] = 0
        record['status'] = 'FAILURE'
        # Create a fake record in mongo
        id = self.images.insert(record)
        assert id is not None
        self.m.update_states()
        rec = self.images.find_one({'_id': id})
        assert rec is None

    def test_lookup(self):
        record = self.good_record()
        # Create a fake record in mongo
        self.images.insert(record)
        i = self.query.copy()
        session = self.m.new_session(self.auth, self.system)
        l = self.m.lookup(session, i)
        assert 'status' in l
        assert '_id' in l
        assert self.m.get_state(l['_id']) == 'READY'
        i = self.query.copy()
        r = self.images.find_one({'_id': l['_id']})
        assert 'expiration' in r
        assert r['expiration'] > time.time()
        i['tag'] = 'bogus'
        l = self.m.lookup(session, i)
        assert l is None

    def test_list(self):
        record = self.good_record()
        # Create a fake record in mongo
        id1 = self.images.insert(record.copy())
        # rec2 is a failed pull, it shouldn't be listed
        rec2 = record.copy()
        rec2['status'] = 'FAILURE'
        session = self.m.new_session(self.auth, self.system)
        li = self.m.imglist(session, self.system)
        assert len(li) == 1
        l = li[0]
        assert '_id' in l
        assert self.m.get_state(l['_id']) == 'READY'
        assert l['_id'] == id1

    def test_repull(self):
        # Test a repull
        record = self.good_record()

        # Create a fake record in mongo
        id = self.images.insert(record)
        assert id is not None
        pr = self.pull
        session = self.m.new_session(self.auth, self.system)
        pull = self.m.pull(session, pr)
        assert pull is not None
        self.assertEqual(pull['status'], 'READY')

    def test_repull_pr(self):
        # Test a repull
        record = self.good_record()

        # Create a fake record in mongo
        id = self.images.insert(record)
        assert id is not None

        # Create a pull record
        pr = self.good_pullrecord()
        pr['status'] = 'SUCCESS'
        id = self.images.insert(pr)
        assert id is not None

        # Now let's try pulling it
        session = self.m.new_session(self.auth, self.system)
        pull = self.m.pull(session, self.pull)
        assert pull is not None
        self.assertEqual(pull['status'], 'READY')

    def test_repull_pr_pulling(self):
        # Test a repull
        record = self.good_record()

        # Create a fake record in mongo
        id = self.images.insert(record)
        assert id is not None

        # Create a pull record
        pr = self.good_pullrecord()
        pr['status'] = 'PULLING'
        id = self.images.insert(pr)
        assert id is not None

        # Now let's try pulling it
        session = self.m.new_session(self.auth, self.system)
        pull = self.m.pull(session, self.pull)
        assert pull is not None
        self.assertEqual(pull['status'], 'PULLING')

    def test_pull_testimage(self):

        # Use defaults for format, arch, os, ostcount, replication
        pr = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'scanon/shanetest:latest',
            'remotetype': 'dockerv2',
            'userACL': [],
            'groupACL': []
        }
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr, testmode=1)  # ,delay=False)
        assert rec is not None
        assert '_id' in rec
        id = rec['_id']
        # Re-pull the same thing.  Should give the same record
        rec = self.m.pull(session, pr, testmode=1)  # ,delay=False)
        assert rec is not None
        assert '_id' in rec
        id2 = rec['_id']
        assert id == id2
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': {'$in': ['scanon/shanetest:latest']}}
        mrec = self.images.find_one(q)
        assert '_id' in mrec
        # Track through transistions
        state = self.time_wait(id)
        assert state == 'READY'
        imagerec = self.m.lookup(session, pr)
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec

    def test_pull(self):
        """
        Basic pull test including an induced pull failure.
        """
        # Use defaults for format, arch, os, ostcount, replication
        pr = self.pull
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr, testmode=1)  # ,delay=False)
        assert rec is not None
        id = rec['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype, 'pulltag': self.tag}
        mrec = self.images.find_one(q)
        assert '_id' in mrec
        # Track through transistions
        state = self.time_wait(id)
        self.assertEquals(state, 'READY')
        imagerec = self.m.lookup(session, self.pull)
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec
        # Cause a failure
        self.images.drop()
        rec = self.m.pull(session, pr, testmode=2)
        time.sleep(10)
        assert rec is not None
        id = rec['_id']
        state = self.m.get_state(id)
        self.assertEquals(state, 'FAILURE')

    def test_pull2(self):
        """
        Test pulling two different images
        """

        # Use defaults for format, arch, os, ostcount, replication
        pr = self.pull
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec1 = self.m.pull(session, pr, testmode=1)  # ,delay=False)
        pr['tag'] = self.tag2
        rec2 = self.m.pull(session, pr, testmode=1)  # ,delay=False)
        assert rec1 is not None
        id1 = rec1['_id']
        assert rec2 is not None
        id2 = rec2['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype, 'pulltag': self.tag}
        mrec = self.images.find_one(q)
        assert '_id' in mrec
        state = self.time_wait(id1)
        assert state == 'READY'
        state = self.time_wait(id2)
        assert state == 'READY'
        mrec = self.images.find_one(q)
        self.images.drop()

    def test_checkread(self):
        """
        Let's simulate various permissions and test them.
        """
        user1 = {'uid': 1, 'gid': 1}
        self.assertTrue(self.m._checkread(user1, {}))
        mock_image_rec = {
            'userACL': None,
            'groupACL': None
        }
        # Test a public image with ACLs set to None
        assert self.m._checkread(user1, mock_image_rec)
        # Now empty list instead of None.  Treat it the same way.
        mock_image_rec['userACL'] = []
        mock_image_rec['groupACL'] = []
        assert self.m._checkread(user1, mock_image_rec)
        assert self.m._checkread(user1, {'private': False})
        # Private false should trump other things
        assert self.m._checkread(user1, {'private': False, 'userACL': [2]})
        assert self.m._checkread(user1, {'private': False, 'groupACL': [2]})
        # Now check a protected image that the user should
        # have access to
        mock_image_rec['userACL'] = [1]
        assert self.m._checkread(user1, mock_image_rec)
        # And Not
        assert self.m._checkread({'uid': 2, 'gid': 1}, mock_image_rec) is False
        # Now check by groupACL
        mock_image_rec['groupACL'] = [1]
        assert self.m._checkread({'uid': 3, 'gid': 1}, mock_image_rec)
        # And Not
        assert self.m._checkread({'uid': 3, 'gid': 2}, mock_image_rec) is False
        # What about an image with a list
        mock_image_rec = {
            'userACL': [1, 2, 3],
            'groupACL': [4, 5, 6]
        }
        assert self.m._checkread(user1, mock_image_rec)
        # And Not
        assert self.m._checkread({'uid': 7, 'gid': 7}, mock_image_rec) is False

    def test_pulls_acl_change(self):
        """
        This simulates a pull inflight + an ACL pull
        request at the same time.
        """
        record = self.good_pullrecord()
        record['status'] = 'PULLING'
        id = self.images.insert(record)
        assert id is not None
        # Now try to submit an ACL change
        session = self.m.new_session(self.auth, self.system)
        pr = {
            'system': record['system'],
            'itype': record['itype'],
            'tag': record['pulltag'],
            'remotetype': 'dockerv2',
            'userACL': [1001, 1002],
            'groupACL': [1003, 1004]
        }
        rec = self.m.pull(session, pr)  # ,delay=False)
        assert rec['status'] == 'PULLING'

    def test_pull_logic(self):
        """
        Consolidate some of the various tests around
        handling various pull sceanrios
        """
        # Assume the image is already recently pulled
        record = self.good_record()
        tag = record['tag'][0]
        basepr = {
            'system': record['system'],
            'itype': record['itype'],
            'tag': tag,
            'remotetype': 'dockerv2',
        }
        id = self.images.insert(record)
        assert id is not None
        session = self.m.new_session(self.auth, self.system)
        pr = basepr.copy()
        rec = self.m.pull(session, pr)  # ,delay=False)
        assert rec['status'] == 'READY'

        # reset and test a re-pull of an old image
        self.images.remove({})
        record['last_pull'] = record['last_pull'] - 36000
        id = self.images.insert(record)
        assert id is not None
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        assert rec['status'] == 'INIT'

        # Re-pull of new image with ACL change
        self.images.remove({})
        pr = basepr.copy()
        id = self.images.insert(record)
        assert id is not None
        pr['userACL'] = [1001]
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        assert rec['status'] == 'INIT'

        # reset and test a re-pull of an old image
        self.images.remove({})
        pr = basepr.copy()
        record['last_pull'] = record['last_pull'] - 36000
        id = self.images.insert(record)
        assert id is not None
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        assert rec['status'] == 'INIT'
        # Now let's do a re-pull with ACL change.  We should
        # get back the prev rec.  The status will now be
        # pending because we do an update status
        pr['userACL'] = [1001]
        session = self.m.new_session(self.auth, self.system)
        rec2 = self.m.pull(session, pr)  # ,delay=False)
        assert rec2['_id'] == rec['_id']
        # TODO: Need to find a way to trigger this test now.
        # self.assertEquals(rec2['status'], 'PENDING')

    def test_pull_public_acl(self):
        """
        Pulling a public image with ACLs should ignore the acls.
        """
        # Use defaults for format, arch, os, ostcount, replication
        pr = {
            'system': self.system,
            'itype': self.itype,
            'tag': self.tag,
            'remotetype': 'dockerv2',
            'userACL': [1001, 1002],
            'groupACL': [1003, 1004]
        }
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        id = rec['_id']
        assert rec is not None
        # Confirm record
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': self.tag}
        state = self.time_wait(id)
        mrec = self.images.find_one(q)
        assert '_id' in mrec
        assert 'userACL' in mrec
        self.assertIn('ENV', mrec)
        # Track through transistions
        state = self.time_wait(id)
        assert state == 'READY'
        mrec = self.images.find_one(q)
        self.assertIn('ENV', mrec)
        self.assertIn('private', mrec)
        self.assertFalse(mrec['private'])

    def test_pull_public_acl_token(self):
        """
        Pulling a public image with ACLs should ignore the acls.
        """
        tokens = self.read_tokens()
        if tokens is None:
            print "Skipping private pull tests"
            return
        # Use defaults for format, arch, os, ostcount, replication
        pr = {
            'system': self.system,
            'itype': self.itype,
            'tag': self.public,
            'remotetype': 'dockerv2',
            'userACL': [1001, 1002],
            'groupACL': [1003, 1004]
        }
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        id = rec['_id']
        assert rec is not None
        # Confirm record
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': self.public}
        mrec = self.images.find_one(q)
        assert '_id' in mrec
        assert 'userACL' in mrec
        assert 1001 in mrec['userACL']
        # Track through transistions
        state = self.time_wait(id)
        assert state == 'READY'
        mrec = self.images.find_one(q)
        assert 'private' in mrec
        assert mrec['private'] is False

    def test_pull_acl(self):
        """
        Basic pull test with ACLs testmode image.
        """
        # Use defaults for format, arch, os, ostcount, replication
        pr = {
            'system': self.system,
            'itype': self.itype,
            'tag': self.private,
            'remotetype': 'dockerv2',
            'userACL': [1001, 1002],
            'groupACL': [1003, 1004]
        }
        # Do the pull
        tokens = self.read_tokens()
        if tokens is None:
            print "Skipping private pull tests"
            return

        session = self.m.new_session(self.auth, self.system)
        session['tokens'] = tokens
        rec = self.m.pull(session, pr)  # ,delay=False)
        self.assertIsNotNone(rec)
        id = rec['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': self.private}
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        self.assertIn('userACL', mrec)
        self.assertIn(1001, mrec['userACL'])
        # Track through transistions
        state = self.time_wait(id)
        self.assertEquals(state, 'READY')
        imagerec = self.m.lookup(session, pr)
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec
        mf = self.get_metafile(self.system, imagerec['id'])
        kv = self.read_metafile(mf)
        assert 'USERACL' in kv
        assert 1001 in kv['USERACL']
        assert 1003 not in kv['USERACL']
        assert 100 in kv['USERACL']
        self.set_last_pull(id, time.time() - 36000)

        # Now let's pull it again with a new userACL
        pr['userACL'] = [1003, 1002]
        session = self.m.new_session(self.auth, self.system)
        session['tokens'] = self.read_tokens()
        rec = self.m.pull(session, pr)  # ,delay=False)
        self.assertIsNotNone(rec)
        id = rec['_id']
        state = self.time_wait(id)
        self.assertIsNone(state)
        imagerec = self.m.lookup(session, pr)
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec
        assert 1003 in imagerec['userACL']
        kv = self.read_metafile(mf)
        assert 1003 in kv['USERACL']
        # Try pulling the same ACLs in a different order
        self.set_last_pull(id, time.time() - 36000)
        pr['userACL'] = [1002, 1003]
        session = self.m.new_session(self.auth, self.system)
        session['tokens'] = self.read_tokens()
        rec = self.m.pull(session, pr)  # ,delay=False)
        self.assertIsNotNone(rec)
        # Don't wait because it should immediately finish
        self.assertEqual(rec['status'], 'READY')
        kv = self.read_metafile(mf)
        self.images.drop()

    def test_import(self):
        """
        Basic import test
        """
        # Use defaults for format, arch, os, ostcount, replication
        pr = {
            'system': self.system,
            'itype': self.itype,
            'tag': self.tag,
            'remotetype': 'dockerv2',
            'filepath': '/images/test/test.squashfs',
            'format': 'squashfs',
            'userACL': [],
            'groupACL': []
        }
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.mngrimport(session, pr, testmode=1)  # ,delay=False)
        assert rec is not None
        id = rec['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype, 'pulltag': self.tag}
        mrec = self.images.find_one(q)
        assert '_id' in mrec
        # Track through transistions
        state = self.time_wait(id)
        self.assertEquals(state, 'READY')
        imagerec = self.m.lookup(session, pr)
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec

    # TODO: Write a test that tries to update an image the
    # user doesn't have permissions to
    def test_acl_update_denied(self):
        pass

    def test_expire_remote(self):
        system = self.system
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file, metafile = self.create_fakeimage(system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, system)
        er = {'system': system, 'tag': self.tag, 'itype': self.itype}
        rec = self.m.expire(session, er, testmode=1)  # ,delay=False)
        assert rec is not None
        time.sleep(2)
        state = self.m.get_state(id)
        assert state == 'EXPIRED'
        assert os.path.exists(file) is False
        assert os.path.exists(metafile) is False

    def test_expire_local(self):
        record = self.good_record()
        system = 'systemb'
        record['system'] = system
        # Create a fake record in mongo
        id = self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file, metafile = self.create_fakeimage(system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, system)
        er = {'system': system, 'tag': self.tag, 'itype': self.itype}
        rec = self.m.expire(session, er)  # ,delay=False)
        assert rec is not None
        time.sleep(2)
        state = self.m.get_state(id)
        assert state == 'EXPIRED'
        assert os.path.exists(file) is False
        assert os.path.exists(metafile) is False

    def test_expire_noadmin(self):
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.auth, self.system)
        er = {'system': self.system, 'tag': self.tag, 'itype': self.itype}
        rec = self.m.expire(session, er, testmode=1)  # ,delay=False)
        assert rec is not None
        time.sleep(2)
        state = self.m.get_state(id)
        assert state == 'READY'
        assert os.path.exists(file) is True
        assert os.path.exists(metafile) is True

    def test_autoexpire_stuckpull(self):
        record = self.good_pullrecord()
        record['status'] = 'ENQUEUED'
        record['last_pull'] = time.time() - 3000
        id = self.images.insert(record)
        assert id is not None
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)
        state = self.m.get_state(id)
        assert state is None

    def test_autoexpire_recentpull(self):
        record = self.good_pullrecord()
        record['status'] = 'ENQUEUED'
        id = self.images.insert(record)
        assert id is not None
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)
        state = self.m.get_state(id)
        assert state == 'ENQUEUED'

    def test_autoexpire(self):
        record = self.good_record()

        # Make it a candidate for expiration (10 secs too old)
        record['expiration'] = time.time() - 10
        id = self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)  # ,delay=False)
        time.sleep(5)
        state = self.m.get_state(id)
        self.assertEquals(state, 'EXPIRED')
        self.assertFalse(os.path.exists(file))
        self.assertFalse(os.path.exists(metafile))

    def test_autoexpire_dontexpire(self):
        # A new image shouldn't expire
        record = self.good_record()
        record['expiration'] = time.time() + 1000
        id = self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)  # ,delay=False)
        time.sleep(2)
        state = self.m.get_state(id)
        assert state == 'READY'
        assert os.path.exists(file) is True
        assert os.path.exists(metafile) is True

    def test_autoexpire_othersystem(self):
        # A new image shouldn't expire
        record = self.good_record()
        record['expiration'] = time.time() - 10
        record['system'] = 'other'
        id = self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)  # ,delay=False)
        time.sleep(2)
        state = self.m.get_state(id)
        assert state == 'READY'
        assert os.path.exists(file) is True
        assert os.path.exists(metafile) is True

    def test_metrics(self):
        rec = {
            "uid": 100,
            "user": "usera",
            "tag": self.tag,
            "system": self.system,
            "id": self.id,
            "type": self.itype
        }
        # Remove everything
        self.metrics.remove({})
        for _ in xrange(100):
            rec['time'] = time.time()
            self.metrics.insert(rec.copy())
        session = self.m.new_session(self.authadmin, self.system)
        recs = self.m.get_metrics(session, self.system, 10)  # ,delay=False)
        self.assertIsNotNone(recs)
        self.assertEquals(len(recs), 10)
        # Try pulling more records than we have
        recs = self.m.get_metrics(session, self.system, 101)  # ,delay=False)
        self.assertIsNotNone(recs)
        self.assertEquals(len(recs), 100)

    def test_status_thread(self):
        # Stop the existing status thread
        self.m.status_queue.put('stop')
        time.sleep(1)
        # Create a pull record
        record = self.good_record()
        record['pulltag'] = 'bogus'
        record['status'] = 'PULLING'
        rec = self.images.insert(record)
        id = record['_id']
        m = {
            'id': id,
            'state': 'READY',
            'meta': {'response': {'id': 'fakeid'}}
        }
        # Create a fake response and queue it
        self.m.status_queue.put(m)
        self.m.status_queue.put('stop')
        self.m.status_thread()
        rec = self.images.find_one({'_id': id})
        self.assertEquals(rec['status'], 'READY')
        # Now do a meta_only update
        # Let's add a new pull record
        record = self.good_record()
        record['pulltag'] = 'bogus'
        record['status'] = 'PULLING'
        id = self.images.insert(record)
        m = {
            'id': id,
            'state': 'READY',
            'meta': {'response': {'id': 'fakeid'}}
        }
        m['meta']['response']['meta_only'] = True
        m['meta']['response']['userACL'] = 1
        m['meta']['response']['groupACL'] = 1
        m['meta']['response']['private'] = True
        self.m.status_queue.put(m)
        self.m.status_queue.put('stop')
        self.m.status_thread()
        rec = self.images.find_one()
        self.assertIn('userACL', rec)
        self.assertTrue(rec['private'])


if __name__ == '__main__':
    unittest.main()
