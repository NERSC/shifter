import os
import pytest
import time
import json
import base64
from copy import deepcopy
from time import sleep
from pymongo import MongoClient
from multiprocessing.pool import ThreadPool
from random import randint
from shifter_imagegw.config import Config
from shifter_imagegw.models import Session, Request
from shifter_imagegw.imageworker import WorkerThreads
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


class mock_worker(WorkerThreads):
    def __init__(self, q):
        self.mode = 1
        self.q = q
        self.pools = ThreadPool(processes=2)
        self.op = "pull"

    def set_mode(self, mode):
        self.mode = mode

    def updater(self, ident, state, meta):
        """
        Updater function: This just post a message to a queue.
        """
        self.q.put({'id': ident, 'state': state, 'meta': meta})

    def pull(self, request, updater):
        if self.mode == 2:
            raise OSError('task failed')
        states = ('PULLING', 'EXAMINATION', 'CONVERSION', 'TRANSFER')
        for state in states:
            updater.update_status(state, state)
            sleep(0.1)
        ret = {
            'id': '%x' % randint(0, 100000),
            'entrypoint': ['./blah'],
            'workdir': '/root',
            'env': ['FOO=bar', 'BAZ=boz']
        }
        state = 'READY'
        updater.update_status(state, state, ret)
        return ret

    def wrkimport(self, request, updater):
        states = ('HASHING', 'TRANSFER', 'READY')
        for state in states:
            updater.update_status(state, state)
            sleep(0.1)
        ret = {
            'id': '%x' % randint(0, 100000),
            'entrypoint': ['./blah'],
            'workdir': '/root',
            'env': ['FOO=bar', 'BAZ=boz']
        }
        state = 'READY'
        updater.update_status(state, state, ret)
        return ret

    def submit(self, req):
        req.updater.update_method = self.updater
        if self.op == "pull":
            self.pools.apply_async(self.pull, [req, req.updater],
                                   {}, None, req.updater.failed)
        elif self.op == "import":
            self.pools.apply_async(self.wrkimport, [req, req.updater],
                                   {}, None, req.updater.failed)


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    monkeypatch.setenv("PATH", f"{test_dir}/fakebin:{os.environ['PATH']}")


@pytest.fixture()
def conf():
    configfile = 'test.json'
    with open(configfile) as config_file:
        data = json.load(config_file)
    config = Config(data)
    yield config


@pytest.fixture(autouse=True)
def ctx():
    class Conf():
        def __init__(self):
            self.configfile = 'test.json'
            with open(self.configfile) as config_file:
                data = json.load(config_file)
            self.config = Config(data)
            mongodb = self.config.MongoDBURI
            client = MongoClient(mongodb)
            db = self.config.MongoDB
            self.images = client[db].images
            self.metrics = client[db].metrics
            self.images.drop()
            self.m = ImageMngr(self.config)
            # Manager with mocked worker
            self.mtm = ImageMngr(self.config)
            self.mtm.workers = mock_worker(self.mtm.status_queue)
            self.system = 'systema'
            self.itype = 'docker'
            self.tag = 'alpine:latest'
            self.id = 'fakeid'
            self.tag2 = 'test2'
            self.public = 'index.docker.io/busybox:latest'
            self.private = 'index.docker.io/scanon/shaneprivate:latest'
            self.format = 'squashfs'
            self.pid = 0
            self.query = {'system': self.system,
                          'itype': self.itype,
                          'tag': self.tag}
            self.pull = Request(system=self.system, itype=self.itype,
                                tag=self.tag, userACL=[], groupACL=[])
            self.session = Session(uid=100, gid=100, system=self.system,
                                   user="user", group="user")
            self.admin_session = Session(uid=0, gid=0, system=self.system,
                                         user="root", group="root")
            # Cleanup Mongo
            self.images.delete_many({})
    conf = Conf()
    yield conf
    conf.m.shutdown()


def time_wait(ctx, id, wstate='READY', TIMEOUT=30):
    poll_interval = 0.5
    count = TIMEOUT / poll_interval
    state = 'UNKNOWN'
    while (state != wstate and count > 0):
        state = ctx.m.db.get_state(id)
        if state is None:
            return None
        count -= 1
        time.sleep(poll_interval)
    return state


def get_metafile(self, system, id):
    idir = self.config.Platforms[system].imageDir

    metafile = os.path.join(idir, f'{id}.meta')
    return metafile


def create_fakeimage(ctx, system, id, format):
    idir = ctx.config.Platforms[system].imageDir

    if os.path.exists(idir) is False:
        os.makedirs(idir)
    file = os.path.join(idir, f'{id}.{format}')
    with open(file, 'w') as f:
        f.write('')
    metafile = os.path.join(idir, f'{id}.meta')
    with open(metafile, 'w') as f:
        f.write('')
    return file, metafile


def good_pullrecord(ctx):
    return {'system': ctx.system,
            'itype': ctx.itype,
            'id': ctx.id,
            'pulltag': ctx.tag,
            'status': 'READY',
            'userACL': [],
            'groupACL': [],
            'ENV': [],
            'ENTRY': '',
            'last_pull': time.time()
            }


def good_record(ctx):
    return {
        'system': ctx.system,
        'itype': ctx.itype,
        'id': ctx.id,
        'tag': [ctx.tag],
        'format': ctx.format,
        'status': 'READY',
        'userACL': [],
        'groupACL': [],
        'last_pull': time.time(),
        'ENV': [],
        'ENTRY': ''
    }


def read_metafile(metafile):
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


def set_last_pull(ctx, id, t):
    ctx.m.images.update({'_id': id}, {'$set': {'last_pull': t}})


def read_tokens():
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
def test_noadmin(ctx):
    s = ctx.session
    assert s
    resp = ctx.m._isadmin(s)
    assert resp is False


def test_admin(ctx):
    s = ctx.admin_session
    assert s
    resp = ctx.m._isadmin(s)
    assert resp is True


def test_add_remove_tag(ctx):
    """
    Create an image and tag with a new tag.
    Make sure both tags show up.
    Remove the tag and make sure the original tags
    doesn't also get removed.
    """
    record = good_record(ctx)
    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id

    assert id
    before = ctx.images.find_one({'_id': id})
    assert before

    # Add a tag a make sure it worked
    status = ctx.m.db.add_tag(id, ctx.system, 'testtag')
    assert status
    after = ctx.images.find_one({'_id': id})
    assert after
    assert 'testtag' in after['tag']
    assert ctx.tag in after['tag']
    # Remove a tag and make sure it worked
    status = ctx.m.db.remove_tag(ctx.system, 'testtag')
    assert status is True
    after = ctx.images.find_one({'_id': id})
    assert after
    assert 'testtag' not in after['tag']


def test_add_remove_two(ctx):
    """
    Test that tag is a list
    """
    record = good_record(ctx)
    # Create a fake record in mongo
    id1 = ctx.images.insert_one(record.copy()).inserted_id
    record['id'] = 'fakeid2'
    record['tag'] = []
    id2 = ctx.images.insert_one(record.copy()).inserted_id

    status = ctx.m.db.add_tag(id2, ctx.system, ctx.tag)
    assert status
    rec1 = ctx.images.find_one({'_id': id1})
    rec2 = ctx.images.find_one({'_id': id2})
    assert ctx.tag not in rec1['tag']
    assert ctx.tag in rec2['tag']


def test_add_same_image_two_system(ctx):
    """
    Test adding tags for the same image on
    different systems.
    """
    record = good_record(ctx)
    record['tag'] = ctx.tag
    # Create a fake record in mongo
    id1 = ctx.images.insert_one(record.copy()).inserted_id
    # add testtag for systema
    status = ctx.m.db.add_tag(id1, ctx.system, 'testtag')
    assert status
    record['system'] = 'systemb'
    id2 = ctx.images.insert_one(record.copy()).inserted_id
    status = ctx.m.db.add_tag(id2, 'systemb', 'testtag')
    assert status
    # Now make sure testtag for first system is still
    # present
    rec = ctx.images.find_one({'_id': id1})
    assert rec
    assert 'testtag' in rec['tag']


def test_add_tagitem(ctx):
    """
    Testing adding a tag
    """
    record = good_record(ctx)
    record['tag'] = ctx.tag
    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id

    status = ctx.m.db.add_tag(id, ctx.system, 'testtag')
    assert status is True
    rec = ctx.images.find_one({'_id': id})
    assert rec
    assert ctx.tag in rec['tag']
    assert 'testtag' in rec['tag']


def test_add_remove_withtag(ctx):
    """
    Test adding a tag and using lookup
    """
    record = good_record(ctx)
    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id

    session = ctx.session
    i = ctx.query.copy()
    status = ctx.m.db.add_tag(id, ctx.system, 'testtag')
    assert status is True
    rec = ctx.m.lookup(session, i['itype'], i['tag'])
    assert rec
    assert ctx.tag in rec['tag']
    assert 'testtag' in rec['tag']


def test_resetexp(ctx):
    record = {'system': ctx.system,
              'itype': ctx.itype,
              'id': ctx.id,
              'pulltag': ctx.tag,
              'status': 'READY',
              'userACL': [],
              'groupACL': [],
              'ENV': [],
              'ENTRY': '',
              'last_pull': 0
              }
    id = ctx.images.insert_one(record.copy()).inserted_id
    assert id
    expire = ctx.m._resetexpire(id)
    assert expire > time.time()
    rec = ctx.images.find_one({'_id': id})
    assert rec['expiration'] == expire


def test_pullable(ctx):
    # An old READY image
    rec = {'last_pull': 0, 'status': 'READY'}
    assert ctx.m._pullable(rec) is True
    rec = {'last_pull': time.time(), 'status': 'READY'}
    # A recent READY image
    assert ctx.m._pullable(rec) is False

    rec = {'last_pull': time.time(),
           'last_heartbeat': 0,
           'status': 'READY'}
    # A recent READY image but an old heartbeat (maybe re-pulled)
    assert ctx.m._pullable(rec) is False

    # A failed image
    rec = {'last_pull': 0, 'status': 'FAILURE'}
    assert ctx.m._pullable(rec) is True
    # recent pull
    rec = {'last_pull': time.time(), 'status': 'FAILURE'}
    assert ctx.m._pullable(rec) is False

    # A hung pull
    rec = {'last_pull': 0, 'last_heartbeat': time.time() - 7200,
           'status': 'PULLING'}
    assert ctx.m._pullable(rec) is True
    # recent pull
    rec = {'last_pull': time.time(), 'status': 'PULLING'}
    assert ctx.m._pullable(rec) is False

    # A hung pull
    rec = {'last_pull': 0, 'last_heartbeat': time.time(),
           'status': 'PULLING'}
    assert ctx.m._pullable(rec) is False


def test_complete_pull(ctx):
    # Test complete_pull
    record = {'system': ctx.system,
              'itype': ctx.itype,
              'id': ctx.id,
              'pulltag': ctx.tag,
              'status': 'READY',
              'userACL': [],
              'groupACL': [],
              'ENV': [],
              'ENTRY': '',
              'last_pull': 0
              }
    record = good_pullrecord(ctx)
    record['last_pull'] = 0
    # Create a fake record in mongo
    # First test when there is no existing image
    id = ctx.images.insert_one(record.copy()).inserted_id
    assert id
    resp = {'id': id, 'tag': ctx.tag}
    ctx.m.complete_pull(id, resp)
    rec = ctx.images.find_one({'_id': id})
    assert rec
    assert rec['tag'] == [ctx.tag]
    assert rec['last_pull'] > 0
    # Create an identical request and
    # run complete again
    id2 = ctx.images.insert_one(record.copy()).inserted_id
    assert id2
    ctx.m.complete_pull(id2, resp)
    # confirm that the record was removed
    rec2 = ctx.images.find_one({'_id': id2})
    assert rec2 is None


def test_update_states(ctx):
    # Test a repull
    record = good_record(ctx)
    record['last_pull'] = 0
    record['status'] = 'FAILURE'
    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id
    assert id
    ctx.m.db.update_states()
    rec = ctx.images.find_one({'_id': id})
    assert rec is None


def test_lookup(ctx):
    assert ctx.m.db.metrics
    record = good_record(ctx)
    # Create a fake record in mongo
    ctx.images.insert_one(record).inserted_id
    i = ctx.query.copy()
    session = ctx.session
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert 'status' in lup
    assert '_id' in lup
    assert ctx.m.db.get_state(lup['_id']) == 'READY'
    i = ctx.query.copy()
    r = ctx.images.find_one({'_id': lup['_id']})
    assert 'expiration' in r
    assert r['expiration'] > time.time()
    i['tag'] = 'bogus'
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert lup is None
    assert len(ctx.m.get_metrics(ctx.admin_session, 10)) > 0


def test_lookup_acl(ctx):
    # Image is protected and user isn't in list
    record = good_record(ctx)
    record['userACL'] = [10]
    record['groupACL'] = [10]
    id = ctx.images.insert_one(record).inserted_id
    i = ctx.query.copy()
    session = ctx.session
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert not lup
    ctx.images.delete_one({'_id': id})

    # Image isn't private
    record['private'] = False
    record['groupACL'] = [10]
    session = ctx.session
    session.uid = 1
    id = ctx.images.insert_one(record).inserted_id
    i = ctx.query.copy()
    session = ctx.session
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert lup
    ctx.images.delete_one({'_id': id})

    # User in user list
    record = good_record(ctx)
    record['userACL'] = [1]
    record['groupACL'] = [10]
    session = ctx.session
    session.uid = 1
    id = ctx.images.insert_one(record).inserted_id
    i = ctx.query.copy()
    session = ctx.session
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert lup
    ctx.images.delete_one({'_id': id})

    # User group in group list
    record = good_record(ctx)
    record['userACL'] = [1]
    record['groupACL'] = [10]
    session = ctx.session
    session.uid = 10
    session.gid = 10
    id = ctx.images.insert_one(record).inserted_id
    i = ctx.query.copy()
    session = ctx.session
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert lup
    ctx.images.delete_one({'_id': id})

    # ACL are none or empty
    record = good_record(ctx)
    record['userACL'] = None
    record['groupACL'] = None
    session = ctx.session
    id = ctx.images.insert_one(record).inserted_id
    i = ctx.query.copy()
    session = ctx.session
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert lup
    ctx.images.delete_one({'_id': id})
    record['userACL'] = []
    record['groupACL'] = []
    id = ctx.images.insert_one(record).inserted_id
    i = ctx.query.copy()
    session = ctx.session
    lup = ctx.m.lookup(session, i['itype'], i['tag'])
    assert lup
    ctx.images.delete_one({'_id': id})


def test_list(ctx):
    record = good_record(ctx)
    # Create a fake record in mongo
    id1 = ctx.images.insert_one(record.copy()).inserted_id
    # rec2 is a failed pull, it shouldn't be listed
    rec2 = record.copy()
    rec2['status'] = 'FAILURE'
    session = ctx.session
    li = ctx.m.imglist(session)
    assert len(li) == 1
    lup = li[0]
    assert '_id' in lup
    assert ctx.m.db.get_state(lup['_id']) == 'READY'
    assert lup['_id'] == id1


def test_repull(ctx):
    # Test a repull
    record = good_record(ctx)

    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id
    assert id
    pr = ctx.pull
    session = ctx.session
    pull = ctx.m.pull(session, pr)
    assert pull
    assert pull['status'] == 'READY'


def test_repull_pr(ctx):
    # Test a repull
    record = good_record(ctx)

    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id
    assert id

    # Create a pull record
    pr = good_pullrecord(ctx)
    pr['status'] = 'SUCCESS'
    id = ctx.images.insert_one(pr).inserted_id
    assert id

    # Now let's try pulling it
    session = ctx.session
    pull = ctx.m.pull(session, ctx.pull)
    assert pull
    assert pull['status'] == 'READY'


def test_repull_pr_pulling(ctx):
    # Test a repull
    record = good_record(ctx)

    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id
    assert id

    # Create a pull record
    pr = good_pullrecord(ctx)
    pr['status'] = 'PULLING'
    id = ctx.images.insert_one(pr).inserted_id
    assert id

    # Now let's try pulling it
    session = ctx.session
    pull = ctx.m.pull(session, ctx.pull)
    assert pull
    assert pull['status'] == 'PULLING'


def xtest_pull_testimage(ctx):

    # Use defaults for format, arch, os, ostcount, replication
    pr = {
        'system': ctx.system,
        'itype': ctx.itype,
        'tag': 'scanon/shanetest:latest',
        'userACL': [],
        'groupACL': []
    }
    # Do the pull
    session = ctx.session
    rec = ctx.mtm.pull(session, pr)  # ,delay=False)
    assert rec
    assert '_id' in rec
    id = rec['_id']
    # Re-pull the same thing.  Should give the same record
    rec = ctx.mtm.pull(session, pr)  # ,delay=False)
    assert rec
    assert '_id' in rec
    id2 = rec['_id']
    assert id == id2
    q = {'system': ctx.system, 'itype': ctx.itype,
         'pulltag': {'$in': ['scanon/shanetest:latest']}}
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    # Track through transistions
    state = time_wait(ctx, id)
    assert state == 'READY'
    imagerec = ctx.mtm.lookup(session, pr)
    assert 'ENTRY' in imagerec
    assert 'ENV' in imagerec


def test_pull_mocked(ctx):
    """
    Basic pull test including an induced pull failure.
    """
    # Use defaults for format, arch, os, ostcount, replication
    pr = ctx.pull
    # Do the pull
    session = ctx.session
    rec = ctx.mtm.pull(session, pr)  # ,delay=False)
    assert rec
    id = rec['_id']
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype, 'pulltag': ctx.tag}
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    # Track through transistions
    state = time_wait(ctx, id)
    assert state == 'READY'
    imagerec = ctx.mtm.lookup(session, pr.itype, pr.tag)
    assert 'ENTRY' in imagerec
    assert 'ENV' in imagerec
    # Cause a failure
    ctx.images.delete_many({})
    ctx.mtm.workers.set_mode(2)
    rec = ctx.mtm.pull(session, pr)
    time.sleep(2)
    assert rec
    id = rec['_id']
    state = ctx.mtm.db.get_state(id)
    assert state == 'FAILURE'
    ctx.mtm.workers.set_mode(1)


def test_pull2(ctx):
    """
    Test pulling two different images
    """

    # Use defaults for format, arch, os, ostcount, replication
    pr = ctx.pull
    # Do the pull
    session = ctx.session
    rec1 = ctx.mtm.pull(session, pr)
    pr.tag = ctx.tag2
    rec2 = ctx.mtm.pull(session, pr)
    assert rec1
    id1 = rec1['_id']
    assert rec2
    id2 = rec2['_id']
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype, 'pulltag': ctx.tag}
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    state = time_wait(ctx, id1)
    assert state == 'READY'
    state = time_wait(ctx, id2)
    assert state == 'READY'
    mrec = ctx.images.find_one(q)
    ctx.images.delete_many({})


def xtest_checkread(ctx):
    """
    Let's simulate various permissions and test them.
    """
    user1 = {'uid': 1, 'gid': 1}
    assert ctx.m._checkread(user1, {}) is True
    mock_image = {
        'userACL': None,
        'groupACL': None
    }
    # Test a public image with ACLs set to None
    assert ctx.m._checkread(user1, mock_image) is True
    # Now empty list instead of None.  Treat it the same way.
    mock_image['userACL'] = []
    mock_image['groupACL'] = []
    assert ctx.m._checkread(user1, mock_image) is True
    assert ctx.m._checkread(user1, {'private': False}) is True
    # Private false should trump other things
    assert ctx.m._checkread(user1, {'private': False, 'userACL': [2]}) is True
    assert ctx.m._checkread(user1, {'private': False, 'groupACL': [2]}) is True
    # Now check a protected image that the user should
    # have access to
    mock_image['userACL'] = [1]
    assert ctx.m._checkread(user1, mock_image) is True
    # And Not
    assert ctx.m._checkread({'uid': 2, 'gid': 1}, mock_image) is False
    # Now check by groupACL
    mock_image['groupACL'] = [1]
    assert ctx.m._checkread({'uid': 3, 'gid': 1}, mock_image) is True
    # And No
    assert ctx.m._checkread({'uid': 3, 'gid': 2}, mock_image) is False
    # What about an image with a list
    mock_image = {
        'userACL': [1, 2, 3],
        'groupACL': [4, 5, 6]
    }
    assert ctx.m._checkread(user1, mock_image) is True
    # And Not
    assert ctx.m._checkread({'uid': 7, 'gid': 7}, mock_image) is False


def test_pulls_acl_change(ctx):
    """
    This simulates a pull inflight + an ACL pull
    request at the same time.
    """
    record = good_pullrecord(ctx)
    record['status'] = 'PULLING'
    id = ctx.images.insert_one(record).inserted_id
    assert id
    # Now try to submit an ACL change
    session = ctx.session
    pr = Request(system=record['system'],
                 itype=record['itype'],
                 tag=record['pulltag'],
                 userACL=[1001, 1002],
                 groupACL=[1003, 1004])
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec['status'] == 'PULLING'


def test_pull_logic(ctx):
    """
    Consolidate some of the various tests around
    handling various pull sceanrios
    """
    # Assume the image is already recently pulled
    record = good_record(ctx)
    system = record['system']
    itype = record['itype']
    tag = record['tag'][0]
    id = ctx.images.insert_one(record).inserted_id
    assert id
    session = ctx.session
    pr = Request(system=system,
                 itype=itype,
                 tag=tag)
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec['status'] == 'READY'

    # reset and test a re-pull of an old image
    ctx.images.delete_many({})
    record['last_pull'] = record['last_pull'] - 36000
    id = ctx.images.insert_one(record).inserted_id
    assert id
    session = ctx.session
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec['status'] == 'INIT'

    # Re-pull of new image with ACL change
    ctx.images.delete_many({})
    id = ctx.images.insert_one(record).inserted_id
    assert id
    pr = Request(system=system,
                 itype=itype,
                 tag=tag)
    pr.userACL = [1001]
    session = ctx.session
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec['status'] == 'INIT'

    # reset and test a re-pull of an old image
    ctx.images.delete_many({})
    pr = Request(system=system,
                 itype=itype,
                 tag=tag)
    record['last_pull'] = record['last_pull'] - 36000
    id = ctx.images.insert_one(record).inserted_id
    assert id
    session = ctx.session
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec['status'] == 'INIT'
    # Now let's do a re-pull with ACL change.  We should
    # get back the prev rec.  The status will now be
    # pending because we do an update status
    pr.userACL = [1001]
    session = ctx.session
    rec2 = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec2['_id'] == rec['_id']
    # TODO: Need to find a way to trigger this test now.
    # ctx.assertEquals(rec2['status'], 'PENDING')


def test_pull_public_acl(ctx):
    """
    Pulling a public image with ACLs should ignore the acls.
    """
    # Use defaults for format, arch, os, ostcount, replication
    pr = ctx.pull
    pr.userACL = [1001, 1002]
    pr.groupACL = [1003, 1004]
    # Do the pull
    session = ctx.session
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    id = rec['_id']
    assert rec
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype,
         'pulltag': ctx.tag}
    state = time_wait(ctx, id)
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    assert 'userACL' in mrec
    assert 'ENV' in mrec
    # Track through transistions
    state = time_wait(ctx, id)
    assert state == 'READY'
    mrec = ctx.images.find_one(q)
    assert 'ENV' in mrec
    assert 'private' in mrec
    assert mrec['private'] is False


def test_pull_public_acl_token(ctx):
    """
    Pulling a public image with ACLs using a token should ignore the acls.
    """
    tokens = read_tokens()
    if tokens is None:
        print("Skipping private pull tests")
        return
    # Use defaults for format, arch, os, ostcount, replication
    pr = {
        'system': ctx.system,
        'itype': ctx.itype,
        'tag': ctx.public,
        'userACL': [1001, 1002],
        'groupACL': [1003, 1004]
    }
    # Do the pull
    session = ctx.session
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    id = rec['_id']
    assert rec
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype,
         'pulltag': ctx.public}
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    assert 'userACL' in mrec
    assert 1001 in mrec['userACL']
    # Track through transistions
    state = time_wait(ctx, id)
    assert state == 'READY'
    mrec = ctx.images.find_one(q)
    assert 'private' in mrec
    assert mrec['private'] is False


def test_pull_acl(ctx):
    """
    Basic pull test with ACLs image.
    """
    # Use defaults for format, arch, os, ostcount, replication
    pr = {
        'system': ctx.system,
        'itype': ctx.itype,
        'tag': ctx.private,
        'userACL': [1001, 1002],
        'groupACL': [1003, 1004]
    }
    # Do the pull
    tokens = read_tokens()
    if tokens is None:
        print("Skipping private pull tests")
        return

    session = ctx.session
    session['tokens'] = tokens
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec
    id = rec['_id']
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype,
         'pulltag': ctx.private}
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    assert 'userACL' in mrec
    assert 1001 in mrec['userACL']
    # Track through transistions
    state = time_wait(ctx, id)
    assert state == 'READY'
    imagerec = ctx.m.lookup(session, pr['itype'], pr['tag'])
    assert 'ENTRY' in imagerec
    assert 'ENV' in imagerec
    mf = ctx.get_metafile(ctx.system, imagerec['id'])
    kv = read_metafile(mf)
    assert 'USERACL' in kv
    assert 1001 in kv['USERACL']
    assert 1003 not in kv['USERACL']
    assert 100 in kv['USERACL']
    ctx.set_last_pull(id, time.time() - 36000)

    # Now let's pull it again with a new userACL
    pr['userACL'] = [1003, 1002]
    session = ctx.session
    session['tokens'] = read_tokens()
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec
    id = rec['_id']
    state = time_wait(ctx, id)
    assert state is None
    imagerec = ctx.m.lookup(session, pr['itype'], pr['tag'])
    assert 'ENTRY' in imagerec
    assert 'ENV' in imagerec
    assert 1003 in imagerec['userACL']
    kv = read_metafile(mf)
    assert 1003 in kv['USERACL']
    # Try pulling the same ACLs in a different order
    ctx.set_last_pull(id, time.time() - 36000)
    pr['userACL'] = [1002, 1003]
    session = ctx.session
    session['tokens'] = read_tokens()
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    assert rec
    # Don't wait because it should immediately finish
    assert rec['status'] == 'READY'
    kv = read_metafile(mf)
    ctx.images.delete_many({})


def test_import(ctx):
    """
    Basic import test
    """
    # Use defaults for format, arch, os, ostcount, replication
    pr = {
        'system': ctx.system,
        'itype': ctx.itype,
        'tag': ctx.tag,
        'filepath': '/images/test/test.squashfs',
        'format': 'squashfs',
        'userACL': [],
        'groupACL': []
    }
    # Do the pull
    session = ctx.session
    ctx.mtm.workers.op = "import"
    rec = ctx.mtm.mngrimport(session, pr)  # ,delay=False)
    assert rec
    id = rec['_id']
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype, 'pulltag': ctx.tag}
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    # Track through transistions
    state = time_wait(ctx, id)
    assert state == 'READY'
    imagerec = ctx.mtm.lookup(session, pr['itype'], pr['tag'])
    assert 'ENTRY' in imagerec
    assert 'ENV' in imagerec


# TODO: Write a test that tries to update an image the
# user doesn't have permissions to
def test_acl_update_denied(ctx):
    pass


def test_expire_local(ctx):
    record = good_record(ctx)
    system = 'systemb'
    record['system'] = system
    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id
    assert id
    # Create a bogus image file
    file, metafile = create_fakeimage(ctx, system, record['id'],
                                      ctx.format)
    session = ctx.admin_session
    er = {'system': system, 'tag': ctx.tag, 'itype': ctx.itype}
    rec = ctx.m.expire(session, er)
    assert rec
    time.sleep(2)
    state = ctx.m.db.get_state(id)
    assert state == 'EXPIRED'
    assert os.path.exists(file) is False
    assert os.path.exists(metafile) is False


def test_expire_noadmin(ctx):
    record = good_record(ctx)
    # Create a fake record in mongo
    id = ctx.images.insert_one(record).inserted_id
    assert id
    # Create a bogus image file
    file, metafile = create_fakeimage(ctx, ctx.system, record['id'],
                                      ctx.format)
    session = ctx.session
    er = {'system': ctx.system, 'tag': ctx.tag, 'itype': ctx.itype}
    _ = ctx.m.expire(session, er)  # ,delay=False)
    # assert rec
    time.sleep(2)
    state = ctx.m.db.get_state(id)
    assert state == 'READY'
    assert os.path.exists(file)
    assert os.path.exists(metafile)


def test_autoexpire_stuckpull(ctx):
    record = good_pullrecord(ctx)
    record['status'] = 'PENDING'
    record['last_pull'] = time.time() - 3000
    id = ctx.images.insert_one(record).inserted_id
    assert id
    session = ctx.admin_session
    ctx.m.autoexpire(session)
    state = ctx.m.db.get_state(id)
    assert state is None


def test_autoexpire_recentpull(ctx):
    record = good_pullrecord(ctx)
    record['status'] = 'PENDING'
    id = ctx.images.insert_one(record).inserted_id
    assert id
    session = ctx.admin_session
    ctx.m.autoexpire(session)
    state = ctx.m.db.get_state(id)
    assert state == 'PENDING'


def test_autoexpire(ctx):
    record = good_record(ctx)

    # Make it a candidate for expiration (10 secs too old)
    record['expiration'] = time.time() - 10
    id = ctx.images.insert_one(record).inserted_id
    assert id
    # Create a bogus image file
    file, metafile = create_fakeimage(ctx, ctx.system, record['id'],
                                      ctx.format)
    session = ctx.admin_session
    ctx.m.autoexpire(session)  # ,delay=False)
    time.sleep(1)
    state = ctx.m.db.get_state(id)
    assert state == 'EXPIRED'
    assert os.path.exists(file) is False
    assert os.path.exists(metafile) is False
    resp = ctx.m.autoexpire(ctx.session)
    assert resp is False


def test_autoexpire_missing(ctx):
    record = good_record(ctx)

    # Make it a candidate for expiration (10 secs too old)
    record.pop('last_pull')
    record['status'] = 'PULLING'
    record['pulltag'] = 'foo'
    record['expiration'] = time.time() - 10
    id = ctx.images.insert_one(record).inserted_id
    assert id
    session = ctx.admin_session
    ctx.m.autoexpire(session)  # ,delay=False)
    state = ctx.m.db.get_state(id)
    assert state == 'PULLING'


def test_autoexpire_dontexpire(ctx):
    # A new image shouldn't expire
    record = good_record(ctx)
    record['expiration'] = time.time() + 1000
    id = ctx.images.insert_one(record).inserted_id
    assert id
    # Create a bogus image file
    file, metafile = create_fakeimage(ctx, ctx.system, record['id'],
                                      ctx.format)
    session = ctx.admin_session
    ctx.m.autoexpire(session)  # ,delay=False)
    time.sleep(2)
    state = ctx.m.db.get_state(id)
    assert state == 'READY'
    assert os.path.exists(file) is True
    assert os.path.exists(metafile) is True


def test_autoexpire_othersystem(ctx):
    # A new image shouldn't expire
    record = good_record(ctx)
    record['expiration'] = time.time() - 10
    record['system'] = 'other'
    id = ctx.images.insert_one(record).inserted_id
    assert id
    # Create a bogus image file
    file, metafile = create_fakeimage(ctx, ctx.system, record['id'],
                                      ctx.format)
    session = ctx.admin_session
    ctx.m.autoexpire(session)  # ,delay=False)
    time.sleep(2)
    state = ctx.m.db.get_state(id)
    assert state == 'READY'
    assert os.path.exists(file) is True
    assert os.path.exists(metafile) is True


def test_metrics(ctx):
    rec = {
        "uid": 100,
        "user": "usera",
        "tag": ctx.tag,
        "system": ctx.system,
        "id": ctx.id,
        "type": ctx.itype
    }
    # Remove everything
    assert ctx.metrics is not None
    ctx.metrics.delete_many({})
    for _ in range(100):
        rec['time'] = time.time()
        ctx.metrics.insert_one(rec.copy())
    session = ctx.admin_session
    recs = ctx.m.get_metrics(session, 10)  # ,delay=False)
    assert recs
    assert len(recs) == 10
    # Try pulling more records than we have
    recs = ctx.m.get_metrics(session, 101)  # ,delay=False)
    assert recs
    assert len(recs) == 100

    session = ctx.session
    recs = ctx.m.get_metrics(session, 10)  # ,delay=False)
    assert recs == []

    session = ctx.admin_session
    ctx.m.db.metrics = None
    recs = ctx.m.get_metrics(session, 10)  # ,delay=False)
    assert recs == []
    ctx.m.db.metrics = True


def test_status_thread(ctx):
    # Stop the existing status thread
    ctx.m.status_queue.put('stop')
    time.sleep(1)
    # Create a pull record
    record = good_record(ctx)
    record['pulltag'] = 'bogus'
    record['status'] = 'PULLING'
    rec = ctx.images.insert_one(record).inserted_id
    id = record['_id']
    m = {
        'id': id,
        'state': 'READY',
        'meta': {'response': {'id': 'fakeid'}}
    }
    # Create a fake response and queue it
    ctx.m.status_queue.put(m)
    ctx.m.status_queue.put('stop')
    ctx.m.status_thread(config=ctx.config)
    rec = ctx.images.find_one({'_id': id})
    assert rec['status'] == 'READY'
    # Now do a meta_only update
    # Let's add a new pull record
    record = good_record(ctx)
    record['pulltag'] = 'bogus'
    record['status'] = 'PULLING'
    id = ctx.images.insert_one(record).inserted_id
    m = {
        'id': id,
        'state': 'READY',
        'meta': {'response': {'id': 'fakeid'}}
    }
    m['meta']['response']['meta_only'] = True
    m['meta']['response']['userACL'] = 1
    m['meta']['response']['groupACL'] = 1
    m['meta']['response']['private'] = True
    ctx.m.status_queue.put(m)
    ctx.m.status_queue.put('stop')
    ctx.m.status_thread(ctx.config)
    rec = ctx.images.find_one()
    assert 'userACL' in rec
    rec['private'] is True


def test_pull_multiple_tags(ctx):
    """
    Test pulling an image with multiple tags.
    """
    pr = Request(system=ctx.system, itype=ctx.itype, tag=ctx.public)
    # Do the pull
    session = ctx.session
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    id = rec['_id']
    assert rec
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype,
         'pulltag': ctx.public}
    mrec = ctx.images.find_one(q)
    assert '_id' in mrec
    # Track through transistions
    state = time_wait(ctx, id)
    assert state == 'READY'

    # Now reppull with a different tag for the same image
    newtag = ctx.public.replace('latest', '1')
    pr = Request(system=ctx.system, itype=ctx.itype, tag=newtag)
    rec = ctx.m.pull(session, pr)  # ,delay=False)
    id = rec['_id']
    assert rec
    # Track through transistions
    state = time_wait(ctx, id)
    # Requery the original record
    mrec = ctx.images.find_one(q)
    ctx.public in mrec['tag']
    assert newtag in mrec['tag']


def test_labels(ctx):
    # Need use_external
    conf = deepcopy(ctx.config)
    os.environ['ENABLE_LABELS'] = "1"
    m = ImageMngr(conf)
    # Use defaults for format, arch, os, ostcount, replication
    pr = ctx.pull
    # Do the pull
    session = ctx.session
    rec1 = m.pull(session, pr)  # ,delay=False)
    id1 = rec1['_id']
    # Confirm record
    q = {'system': ctx.system, 'itype': ctx.itype, 'pulltag': ctx.tag}
    state = time_wait(ctx, id1)
    assert state == 'READY'
    mrec = ctx.images.find_one(q)
    assert 'LABELS' in mrec
    assert 'alabel' in mrec['LABELS']
    pr = ctx.query.copy()
    look = m.lookup(session, pr['itype'], pr['tag'])
    assert 'LABELS' in look
    assert 'alabel' in look['LABELS']
    ctx.images.drop()
