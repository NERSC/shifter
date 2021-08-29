import os
import unittest
import time
import json
import base64
import logging
from copy import deepcopy
from pymongo import MongoClient
from nose.plugins.attrib import attr
from shifter_imagegw.imagemngr import ImageMngr

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
            print(("Number of loggers %d" % (len(self.logger.handlers))))
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
            list = [int(x) for x in kv['USERACL'].split(',')]
            kv['USERACL'] = list
        if 'GROUPACL' in kv:
            list = [int(x) for x in kv['GROUPACL'].split(',')]
            kv['GROUPACL'] = list

        return kv

    def set_last_pull(self, id, t):
        self.m.images.update({'_id': id}, {'$set': {'last_pull': t}})

    def read_tokens(self):
        if not os.path.exists("./tokens.cfg"):
            return None
        with open("./tokens.cfg") as f:
            d = json.load(f)
        for k in list(d.keys()):
            d[k] = base64.b64decode(d[k])
        return d

#
#  Tests
#
    @attr('fast')
    def test_session(self):
        s = self.m.new_session(self.auth, self.system)
        self.assertIsNotNone(s)
        try:
            s = self.m.new_session(self.badauth, self.system)
        except:
            pass

    @attr('fast')
    def test_noadmin(self):
        s = self.m.new_session(self.auth, self.system)
        self.assertIsNotNone(s)
        resp = self.m._isadmin(s, self.system)
        self.assertFalse(resp)

    @attr('fast')
    def test_admin(self):
        s = self.m.new_session(self.authadmin, self.system)
        self.assertIsNotNone(s)
        resp = self.m._isadmin(s, self.system)
        self.assertTrue(resp)

    @attr('fast')
    def test_add_remove_tag(self):
        """
        Create an image and tag with a new tag.
        Make sure both tags show up.
        Remove the tag and make sure the original tags
        doesn't also get removed.
        """
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)

        self.assertIsNotNone(id)
        before = self.images.find_one({'_id': id})
        self.assertIsNotNone(before)
        # Add a tag a make sure it worked
        status = self.m.add_tag(id, self.system, 'testtag')
        self.assertTrue(status)
        after = self.images.find_one({'_id': id})
        self.assertIsNotNone(after)
        self.assertIn('testtag', after['tag'])
        self.assertIn(self.tag, after['tag'])
        # Remove a tag and make sure it worked
        status = self.m.remove_tag(self.system, 'testtag')
        self.assertTrue(status)
        after = self.images.find_one({'_id': id})
        self.assertIsNotNone(after)
        self.assertNotIn('testtag', after['tag'])

    @attr('fast')
    def test_add_tagitem(self):
        """
        Testing adding a tag
        """
        record = self.good_record()
        record['tag'] = self.tag
        # Create a fake record in mongo
        id = self.images.insert(record)

        status = self.m.add_tag(id, self.system, 'testtag')
        self.assertTrue(status)
        rec = self.images.find_one({'_id': id})
        self.assertIsNotNone(rec)
        self.assertIn(self.tag, rec['tag'])
        self.assertIn('testtag', rec['tag'])

    @attr('fast')
    def test_add_remove_withtag(self):
        """
        Test adding a tag and using lookup
        """
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)

        session = self.m.new_session(self.auth, self.system)
        i = self.query.copy()
        status = self.m.add_tag(id, self.system, 'testtag')
        self.assertTrue(status)
        rec = self.m.lookup(session, i)
        self.assertIsNotNone(rec)
        self.assertIn(self.tag, rec['tag'])
        self.assertIn('testtag', rec['tag'])

    @attr('fast')
    def test_add_remove_two(self):
        """
        Test that tag is a list
        """
        record = self.good_record()
        # Create a fake record in mongo
        id1 = self.images.insert(record.copy())
        record['id'] = 'fakeid2'
        record['tag'] = []
        id2 = self.images.insert(record.copy())

        status = self.m.add_tag(id2, self.system, self.tag)
        self.assertTrue(status)
        rec1 = self.images.find_one({'_id': id1})
        rec2 = self.images.find_one({'_id': id2})
        self.assertNotIn(self.tag, rec1['tag'])
        self.assertIn(self.tag, rec2['tag'])

    @attr('fast')
    def test_add_same_image_two_system(self):
        """
        Test adding tags for the same image on
        different systems.
        """
        record = self.good_record()
        record['tag'] = self.tag
        # Create a fake record in mongo
        id1 = self.images.insert(record.copy())
        # add testtag for systema
        status = self.m.add_tag(id1, self.system, 'testtag')
        self.assertTrue(status)
        record['system'] = 'systemb'
        id2 = self.images.insert(record.copy())
        status = self.m.add_tag(id2, 'systemb', 'testtag')
        self.assertTrue(status)
        # Now make sure testtag for first system is still
        # present
        rec = self.images.find_one({'_id': id1})
        self.assertIsNotNone(rec)
        self.assertIn('testtag', rec['tag'])

    @attr('fast')
    def test_isasystem(self):
        self.assertTrue(self.m._isasystem(self.system))
        self.assertFalse(self.m._isasystem('bogus'))

    @attr('fast')
    def test_resetexp(self):
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
        self.assertIsNotNone(id)
        expire = self.m._resetexpire(id)
        self.assertGreater(expire, time.time())
        rec = self.images.find_one({'_id': id})
        self.assertEqual(rec['expiration'], expire)

    @attr('fast')
    def test_pullable(self):
        # An old READY image
        rec = {'last_pull': 0, 'status': 'READY'}
        self.assertTrue(self.m._pullable(rec))
        rec = {'last_pull': time.time(), 'status': 'READY'}
        # A recent READY image
        self.assertFalse(self.m._pullable(rec))

        rec = {'last_pull': time.time(),
               'last_heartbeat': 0,
               'status': 'READY'}
        # A recent READY image but an old heartbeat (maybe re-pulled)
        self.assertFalse(self.m._pullable(rec))

        # A failed image
        rec = {'last_pull': 0, 'status': 'FAILURE'}
        self.assertTrue(self.m._pullable(rec))
        # recent pull
        rec = {'last_pull': time.time(), 'status': 'FAILURE'}
        self.assertFalse(self.m._pullable(rec))

        # A hung pull
        rec = {'last_pull': 0, 'last_heartbeat': time.time() - 7200,
               'status': 'PULLING'}
        self.assertTrue(self.m._pullable(rec))
        # recent pull
        rec = {'last_pull': time.time(), 'status': 'PULLING'}
        self.assertFalse(self.m._pullable(rec))

        # A hung pull
        rec = {'last_pull': 0, 'last_heartbeat': time.time(),
               'status': 'PULLING'}
        self.assertFalse(self.m._pullable(rec))

    @attr('fast')
    def test_complete_pull(self):
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
        self.assertIsNotNone(id)
        resp = {'id': id, 'tag': self.tag}
        self.m.complete_pull(id, resp)
        rec = self.images.find_one({'_id': id})
        self.assertIsNotNone(rec)
        self.assertEqual(rec['tag'], [self.tag])
        self.assertGreater(rec['last_pull'], 0)
        # Create an identical request and
        # run complete again
        id2 = self.images.insert(record.copy())
        self.assertIsNotNone(id2)
        self.m.complete_pull(id2, resp)
        # confirm that the record was removed
        rec2 = self.images.find_one({'_id': id2})
        self.assertIsNone(rec2)

    @attr('fast')
    def test_update_states(self):
        # Test a repull
        record = self.good_record()
        record['last_pull'] = 0
        record['status'] = 'FAILURE'
        # Create a fake record in mongo
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        self.m.update_states()
        rec = self.images.find_one({'_id': id})
        self.assertIsNone(rec)

    @attr('fast')
    def test_lookup(self):
        record = self.good_record()
        # Create a fake record in mongo
        self.images.insert(record)
        i = self.query.copy()
        session = self.m.new_session(self.auth, self.system)
        l = self.m.lookup(session, i)
        self.assertIn('status', l)
        self.assertIn('_id', l)
        self.assertEqual(self.m.get_state(l['_id']), 'READY')
        i = self.query.copy()
        r = self.images.find_one({'_id': l['_id']})
        self.assertIn('expiration', r)
        self.assertGreater(r['expiration'], time.time())
        i['tag'] = 'bogus'
        l = self.m.lookup(session, i)
        self.assertIsNone(l)

    @attr('fast')
    def test_list(self):
        record = self.good_record()
        # Create a fake record in mongo
        id1 = self.images.insert(record.copy())
        # rec2 is a failed pull, it shouldn't be listed
        rec2 = record.copy()
        rec2['status'] = 'FAILURE'
        session = self.m.new_session(self.auth, self.system)
        li = self.m.imglist(session, self.system)
        self.assertEqual(len(li), 1)
        l = li[0]
        self.assertIn('_id', l)
        self.assertEqual(self.m.get_state(l['_id']), 'READY')
        self.assertEqual(l['_id'], id1)

    def test_repull(self):
        # Test a repull
        record = self.good_record()

        # Create a fake record in mongo
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        pr = self.pull
        session = self.m.new_session(self.auth, self.system)
        pull = self.m.pull(session, pr)
        self.assertIsNotNone(pull)
        self.assertEqual(pull['status'], 'READY')

    def test_repull_pr(self):
        # Test a repull
        record = self.good_record()

        # Create a fake record in mongo
        id = self.images.insert(record)
        self.assertIsNotNone(id)

        # Create a pull record
        pr = self.good_pullrecord()
        pr['status'] = 'SUCCESS'
        id = self.images.insert(pr)
        self.assertIsNotNone(id)

        # Now let's try pulling it
        session = self.m.new_session(self.auth, self.system)
        pull = self.m.pull(session, self.pull)
        self.assertIsNotNone(pull)
        self.assertEqual(pull['status'], 'READY')

    def test_repull_pr_pulling(self):
        # Test a repull
        record = self.good_record()

        # Create a fake record in mongo
        id = self.images.insert(record)
        self.assertIsNotNone(id)

        # Create a pull record
        pr = self.good_pullrecord()
        pr['status'] = 'PULLING'
        id = self.images.insert(pr)
        self.assertIsNotNone(id)

        # Now let's try pulling it
        session = self.m.new_session(self.auth, self.system)
        pull = self.m.pull(session, self.pull)
        self.assertIsNotNone(pull)
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
        self.assertIsNotNone(rec)
        self.assertIn('_id', rec)
        id = rec['_id']
        # Re-pull the same thing.  Should give the same record
        rec = self.m.pull(session, pr, testmode=1)  # ,delay=False)
        self.assertIsNotNone(rec)
        self.assertIn('_id', rec)
        id2 = rec['_id']
        self.assertEqual(id, id2)
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': {'$in': ['scanon/shanetest:latest']}}
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        # Track through transistions
        state = self.time_wait(id)
        self.assertEqual(state, 'READY')
        imagerec = self.m.lookup(session, pr)
        self.assertIn('ENTRY', imagerec)
        self.assertIn('ENV', imagerec)

    def test_pull(self):
        """
        Basic pull test including an induced pull failure.
        """
        # Use defaults for format, arch, os, ostcount, replication
        pr = self.pull
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr, testmode=1)  # ,delay=False)
        self.assertIsNotNone(rec)
        id = rec['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype, 'pulltag': self.tag}
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        # Track through transistions
        state = self.time_wait(id)
        self.assertEqual(state, 'READY')
        imagerec = self.m.lookup(session, self.pull)
        self.assertIn('ENTRY', imagerec)
        self.assertIn('ENV', imagerec)
        # Cause a failure
        self.images.drop()
        rec = self.m.pull(session, pr, testmode=2)
        time.sleep(10)
        self.assertIsNotNone(rec)
        id = rec['_id']
        state = self.m.get_state(id)
        self.assertEqual(state, 'FAILURE')

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
        self.assertIsNotNone(rec1)
        id1 = rec1['_id']
        self.assertIsNotNone(rec2)
        id2 = rec2['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype, 'pulltag': self.tag}
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        state = self.time_wait(id1)
        self.assertEqual(state, 'READY')
        state = self.time_wait(id2)
        self.assertEqual(state, 'READY')
        mrec = self.images.find_one(q)
        self.images.drop()

    @attr('fast')
    def test_checkread(self):
        """
        Let's simulate various permissions and test them.
        """
        user1 = {'uid': 1, 'gid': 1}
        self.assertTrue(self.m._checkread(user1, {}))
        mock_image = {
            'userACL': None,
            'groupACL': None
        }
        # Test a public image with ACLs set to None
        self.assertTrue(self.m._checkread(user1, mock_image))
        # Now empty list instead of None.  Treat it the same way.
        mock_image['userACL'] = []
        mock_image['groupACL'] = []
        self.assertTrue(self.m._checkread(user1, mock_image))
        self.assertTrue(self.m._checkread(user1, {'private': False}))
        # Private false should trump other things
        self.assertTrue(self.m._checkread(user1,
                                          {'private': False, 'userACL': [2]}))
        self.assertTrue(self.m._checkread(user1,
                                          {'private': False, 'groupACL': [2]}))
        # Now check a protected image that the user should
        # have access to
        mock_image['userACL'] = [1]
        self.assertTrue(self.m._checkread(user1, mock_image))
        # And Not
        self.assertFalse(self.m._checkread({'uid': 2, 'gid': 1}, mock_image))
        # Now check by groupACL
        mock_image['groupACL'] = [1]
        self.assertTrue(self.m._checkread({'uid': 3, 'gid': 1}, mock_image))
        # And Not
        self.assertFalse(self.m._checkread({'uid': 3, 'gid': 2}, mock_image))
        # What about an image with a list
        mock_image = {
            'userACL': [1, 2, 3],
            'groupACL': [4, 5, 6]
        }
        self.assertTrue(self.m._checkread(user1, mock_image))
        # And Not
        self.assertFalse(self.m._checkread({'uid': 7, 'gid': 7}, mock_image))

    def test_pulls_acl_change(self):
        """
        This simulates a pull inflight + an ACL pull
        request at the same time.
        """
        record = self.good_pullrecord()
        record['status'] = 'PULLING'
        id = self.images.insert(record)
        self.assertIsNotNone(id)
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
        self.assertEqual(rec['status'], 'PULLING')

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
        self.assertIsNotNone(id)
        session = self.m.new_session(self.auth, self.system)
        pr = basepr.copy()
        rec = self.m.pull(session, pr)  # ,delay=False)
        self.assertEqual(rec['status'], 'READY')

        # reset and test a re-pull of an old image
        self.images.remove({})
        record['last_pull'] = record['last_pull'] - 36000
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        self.assertEqual(rec['status'], 'INIT')

        # Re-pull of new image with ACL change
        self.images.remove({})
        pr = basepr.copy()
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        pr['userACL'] = [1001]
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        self.assertEqual(rec['status'], 'INIT')

        # reset and test a re-pull of an old image
        self.images.remove({})
        pr = basepr.copy()
        record['last_pull'] = record['last_pull'] - 36000
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        self.assertEqual(rec['status'], 'INIT')
        # Now let's do a re-pull with ACL change.  We should
        # get back the prev rec.  The status will now be
        # pending because we do an update status
        pr['userACL'] = [1001]
        session = self.m.new_session(self.auth, self.system)
        rec2 = self.m.pull(session, pr)  # ,delay=False)
        self.assertEqual(rec2['_id'], rec['_id'])
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
        self.assertIsNotNone(rec)
        # Confirm record
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': self.tag}
        state = self.time_wait(id)
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        self.assertIn('userACL', mrec)
        self.assertIn('ENV', mrec)
        # Track through transistions
        state = self.time_wait(id)
        self.assertEqual(state, 'READY')
        mrec = self.images.find_one(q)
        self.assertIn('ENV', mrec)
        self.assertIn('private', mrec)
        self.assertFalse(mrec['private'])

    def test_pull_public_acl_token(self):
        """
        Pulling a public image with ACLs using a token should ignore the acls.
        """
        tokens = self.read_tokens()
        if tokens is None:
            print("Skipping private pull tests")
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
        self.assertIsNotNone(rec)
        # Confirm record
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': self.public}
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        self.assertIn('userACL', mrec)
        self.assertIn(1001, mrec['userACL'])
        # Track through transistions
        state = self.time_wait(id)
        self.assertEqual(state, 'READY')
        mrec = self.images.find_one(q)
        self.assertIn('private', mrec)
        self.assertFalse(mrec['private'])

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
            print("Skipping private pull tests")
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
        self.assertEqual(state, 'READY')
        imagerec = self.m.lookup(session, pr)
        self.assertIn('ENTRY', imagerec)
        self.assertIn('ENV', imagerec)
        mf = self.get_metafile(self.system, imagerec['id'])
        kv = self.read_metafile(mf)
        self.assertIn('USERACL', kv)
        self.assertIn(1001, kv['USERACL'])
        self.assertNotIn(1003, kv['USERACL'])
        self.assertIn(100, kv['USERACL'])
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
        self.assertIn('ENTRY', imagerec)
        self.assertIn('ENV', imagerec)
        self.assertIn(1003, imagerec['userACL'])
        kv = self.read_metafile(mf)
        self.assertIn(1003, kv['USERACL'])
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
        self.assertIsNotNone(rec)
        id = rec['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype, 'pulltag': self.tag}
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        # Track through transistions
        state = self.time_wait(id)
        self.assertEqual(state, 'READY')
        imagerec = self.m.lookup(session, pr)
        self.assertIn('ENTRY', imagerec)
        self.assertIn('ENV', imagerec)

    # TODO: Write a test that tries to update an image the
    # user doesn't have permissions to
    def test_acl_update_denied(self):
        pass

    def test_expire_remote(self):
        system = self.system
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        # Create a bogus image file
        file, metafile = self.create_fakeimage(system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, system)
        er = {'system': system, 'tag': self.tag, 'itype': self.itype}
        rec = self.m.expire(session, er, testmode=1)  # ,delay=False)
        self.assertIsNotNone(rec)
        time.sleep(2)
        state = self.m.get_state(id)
        self.assertEqual(state, 'EXPIRED')
        self.assertFalse(os.path.exists(file))
        self.assertFalse(os.path.exists(metafile))

    def test_expire_local(self):
        record = self.good_record()
        system = 'systemb'
        record['system'] = system
        # Create a fake record in mongo
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        # Create a bogus image file
        file, metafile = self.create_fakeimage(system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, system)
        er = {'system': system, 'tag': self.tag, 'itype': self.itype}
        rec = self.m.expire(session, er)  # ,delay=False)
        self.assertIsNotNone(rec)
        time.sleep(2)
        state = self.m.get_state(id)
        self.assertEqual(state, 'EXPIRED')
        self.assertFalse(os.path.exists(file))
        self.assertFalse(os.path.exists(metafile))

    def test_expire_noadmin(self):
        record = self.good_record()
        # Create a fake record in mongo
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.auth, self.system)
        er = {'system': self.system, 'tag': self.tag, 'itype': self.itype}
        rec = self.m.expire(session, er, testmode=1)  # ,delay=False)
        self.assertIsNotNone(rec)
        time.sleep(2)
        state = self.m.get_state(id)
        self.assertEqual(state, 'READY')
        self.assertTrue(os.path.exists(file))
        self.assertTrue(os.path.exists(metafile))

    def test_autoexpire_stuckpull(self):
        record = self.good_pullrecord()
        record['status'] = 'PENDING'
        record['last_pull'] = time.time() - 3000
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)
        state = self.m.get_state(id)
        self.assertIsNone(state)

    def test_autoexpire_recentpull(self):
        record = self.good_pullrecord()
        record['status'] = 'PENDING'
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)
        state = self.m.get_state(id)
        self.assertEqual(state, 'PENDING')

    def test_autoexpire(self):
        record = self.good_record()

        # Make it a candidate for expiration (10 secs too old)
        record['expiration'] = time.time() - 10
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)  # ,delay=False)
        time.sleep(5)
        state = self.m.get_state(id)
        self.assertEqual(state, 'EXPIRED')
        self.assertFalse(os.path.exists(file))
        self.assertFalse(os.path.exists(metafile))

    def test_autoexpire_dontexpire(self):
        # A new image shouldn't expire
        record = self.good_record()
        record['expiration'] = time.time() + 1000
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)  # ,delay=False)
        time.sleep(2)
        state = self.m.get_state(id)
        self.assertEqual(state, 'READY')
        self.assertTrue(os.path.exists(file))
        self.assertTrue(os.path.exists(metafile))

    def test_autoexpire_othersystem(self):
        # A new image shouldn't expire
        record = self.good_record()
        record['expiration'] = time.time() - 10
        record['system'] = 'other'
        id = self.images.insert(record)
        self.assertIsNotNone(id)
        # Create a bogus image file
        file, metafile = self.create_fakeimage(self.system, record['id'],
                                               self.format)
        session = self.m.new_session(self.authadmin, self.system)
        self.m.autoexpire(session, self.system, testmode=1)  # ,delay=False)
        time.sleep(2)
        state = self.m.get_state(id)
        self.assertEqual(state, 'READY')
        self.assertTrue(os.path.exists(file))
        self.assertTrue(os.path.exists(metafile))

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
        for _ in range(100):
            rec['time'] = time.time()
            self.metrics.insert(rec.copy())
        session = self.m.new_session(self.authadmin, self.system)
        recs = self.m.get_metrics(session, self.system, 10)  # ,delay=False)
        self.assertIsNotNone(recs)
        self.assertEqual(len(recs), 10)
        # Try pulling more records than we have
        recs = self.m.get_metrics(session, self.system, 101)  # ,delay=False)
        self.assertIsNotNone(recs)
        self.assertEqual(len(recs), 100)

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
        self.assertEqual(rec['status'], 'READY')
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

    def test_pull_multiple_tags(self):
        """
        Test pulling an image with multiple tags.
        """
        pr = {
            'system': self.system,
            'itype': self.itype,
            'tag': self.public,
            'remotetype': 'dockerv2'
        }
        # Do the pull
        session = self.m.new_session(self.auth, self.system)
        rec = self.m.pull(session, pr)  # ,delay=False)
        id = rec['_id']
        self.assertIsNotNone(rec)
        # Confirm record
        q = {'system': self.system, 'itype': self.itype,
             'pulltag': self.public}
        mrec = self.images.find_one(q)
        self.assertIn('_id', mrec)
        # Track through transistions
        state = self.time_wait(id)
        self.assertEqual(state, 'READY')

        # Now reppull with a different tag for the same image
        newtag = self.public.replace('latest', '1')
        pr = {
            'system': self.system,
            'itype': self.itype,
            'tag': newtag,
            'remotetype': 'dockerv2'
        }
        rec = self.m.pull(session, pr)  # ,delay=False)
        id = rec['_id']
        self.assertIsNotNone(rec)
        # Track through transistions
        state = self.time_wait(id)
        # Requery the original record
        mrec = self.images.find_one(q)
        self.assertIn(self.public, mrec['tag'])
        self.assertIn(newtag, mrec['tag'])

    def test_labels(self):
        # Need use_external
        conf = deepcopy(self.config)
        conf['Platforms']['systema']['use_external'] = True
        m = ImageMngr(conf, logger=self.logger)
        # Use defaults for format, arch, os, ostcount, replication
        pr = self.pull
        # Do the pull
        session = m.new_session(self.auth, self.system)
        rec1 = m.pull(session, pr)  # ,delay=False)
        id1 = rec1['_id']
        # Confirm record
        q = {'system': self.system, 'itype': self.itype, 'pulltag': self.tag}
        state = self.time_wait(id1)
        self.assertEqual(state, 'READY')
        mrec = self.images.find_one(q)
        self.assertIn('LABELS', mrec)
        self.assertIn('alabel', mrec['LABELS'])
        look_req = self.query.copy()
        look = m.lookup(session, look_req)
        self.assertIn('LABELS', look)
        self.assertIn('alabel', look['LABELS'])
        self.images.drop()


if __name__ == '__main__':
    unittest.main()
