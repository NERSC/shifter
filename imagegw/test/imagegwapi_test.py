import os
import time
import urllib.request, urllib.parse, urllib.error
import json
from copy import deepcopy
from shifter_imagegw.fasthash import fast_hash
from shifter_imagegw.imagemngr import ImageMngr
from pymongo import MongoClient
import pytest

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


@pytest.fixture(autouse=True)
def set_gwconfig(monkeypatch):
    base_dir = os.path.dirname(os.path.abspath(__file__)) + "/.."
    cfg_test = os.path.abspath(os.path.join(base_dir, 'test.json'))
    cfg_example = os.path.abspath(os.path.join(base_dir, 'test.json.example'))
    configfile = cfg_test if os.path.exists(cfg_test) else cfg_example
    monkeypatch.setenv('GWCONFIG', configfile)
    monkeypatch.setenv('CONFIG', configfile)


@pytest.fixture() #scope='module')
def api_ctx():
    # Resolve config file, prefer test.json if present else fallback
    base_dir = os.path.dirname(os.path.abspath(__file__)) + "/.."
    cfg_test = os.path.abspath(os.path.join(base_dir, 'test.json'))
    cfg_example = os.path.abspath(os.path.join(base_dir, 'test.json.example'))
    configfile = cfg_test if os.path.exists(cfg_test) else cfg_example

    # Import after setting env so api reads correct config
    from shifter_imagegw import api
    api.config['TESTING'] = True

    # Prepare filesystem locations referenced by config
    with open(configfile) as f:
        config = json.load(f)
    mongouri = config['MongoDBURI']
    client = MongoClient(mongouri)
    dbname = config['MongoDB']
    images = client[dbname].images
    metrics = client[dbname].metrics
    images.drop()
    metrics.remove({})

    if not os.path.exists(config['CacheDirectory']):
        os.makedirs(config['CacheDirectory'], exist_ok=True)
    p = config['Platforms']['systema']['ssh']['imageDir']
    if not os.path.exists(p):
        os.makedirs(p, exist_ok=True)

    ctx = {
        'configfile': configfile,
        'config': config,
        'mgr': api.getmgr(),
        'app': api.app.test_client,
        'images': images,
        'metrics': metrics,
        'url': '/api',
        'system': 'systema',
        'itype': 'docker',
        'itag': 'alpine:latest',
        'auth': 'good:user:user::500:500',
        'authadmin': 'good:root:root::0:0',
        'auth_bad': 'bad:user:user::501:501',
        'logfile': '/tmp/worker.log',
        'pid': 0,
        'test_dir': os.path.dirname(os.path.abspath(__file__)) + "/../test/",
    }
    ctx['tag'] = urllib.parse.quote(ctx['itag'])
    ctx['urlreq'] = "%s/%s/%s" % (ctx['system'], ctx['itype'], ctx['tag'])

    yield ctx

    # Teardown
    ctx['mgr'].shutdown()


def time_wait(app, url, auth, urlreq, data=None, state='READY', op='pull', TIMEOUT=30):
    poll_interval = 0.5
    count = TIMEOUT / poll_interval
    cstate = 'UNKNOWN'
    uri = '%s/%s/%s/' % (url, op, urlreq)
    while (cstate != state and count > 0):
        _, rv = app.post(uri, data=data, headers={AUTH_HEADER: auth})
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


def good_record(ctx):
    return {'system': ctx['system'],
            'itype': ctx['itype'],
            'id': 'bogus',
            'tag': [ctx['itag']],
            'format': 'squashfs',
            'status': 'READY',
            'userACL': [],
            'groupACL': [],
            'last_pull': time.time(),
            'ENV': [],
            'ENTRY': '',
            }


def test_pull(api_ctx):
    uri = '%s/pull/%s/' % (api_ctx['url'], api_ctx['urlreq'])
    data = {'allowed_uids': '1000,1001',
            'allowed_gids': '1002,1003'}
    datajson = json.dumps(data)
    app = api_ctx['app']
    _, rv = app.post(uri, headers={AUTH_HEADER: api_ctx['auth']},
                       data=datajson)
    data = rv.json
    assert 'userACL' in data
    assert 'groupACL' in data
    assert 1000 in data['userACL']
    assert 500 in data['userACL']
    assert 500 in data['groupACL']
    assert 1002 in data['groupACL']
    rv = time_wait(app, api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'])
    assert rv.status == 200


def test_list(api_ctx):
    uri = '%s/list/%s/' % (api_ctx['url'], 'systemc')
    _, rv = api_ctx['app'].get(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 404
    rv = time_wait(api_ctx['app'], api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'])
    assert rv.status == 200
    uri = '%s/list/%s/' % (api_ctx['url'], api_ctx['system'])
    _, rv = api_ctx['app'].get(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 200


def test_queue(api_ctx):
    uri = '%s/pull/%s/' % (api_ctx['url'], api_ctx['urlreq'])
    _, rv = api_ctx['app'].post(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 200
    uri = '%s/queue/%s/' % (api_ctx['url'], api_ctx['system'])
    _, rv = api_ctx['app'].get(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 200


def test_pulllookup(api_ctx):
    rv = time_wait(api_ctx['app'], api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'])
    uri = '%s/lookup/%s/' % (api_ctx['url'], api_ctx['urlreq'])
    _, rv = api_ctx['app'].get(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 200
    uri = '%s/lookup/%s/%s/' % (api_ctx['url'], api_ctx['system'], 'bogus')
    _, rv = api_ctx['app'].get(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 404


def test_lookup(api_ctx):
    record = good_record(api_ctx)
    id = api_ctx['images'].insert(record)
    assert id is not None
    uri = '%s/lookup/%s/' % (api_ctx['url'], api_ctx['urlreq'])
    _, rv = api_ctx['app'](uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 200


def test_expire(api_ctx):
    uri = '%s/expire/%s/%s/%s/' % (api_ctx['url'], api_ctx['system'], api_ctx['itype'],
                                   api_ctx['tag'])
    _, rv = api_ctx['app'].get(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 200


def test_autoexpire(api_ctx):
    record = good_record(api_ctx)
    record['expiration'] = time.time() - 100
    id = api_ctx['images'].insert(record)
    uri = '%s/autoexpire/%s/' % (api_ctx['url'], api_ctx['system'])
    _, rv = api_ctx['app'](uri, headers={AUTH_HEADER: api_ctx['authadmin']})
    assert rv.status == 200

    count = 20
    while count > 0:
        _, rv = api_ctx['app'](uri, headers={AUTH_HEADER: api_ctx['authadmin']})
        r = api_ctx['images'].find_one({'_id': id})
        if r['status'] == 'EXPIRED':
            break
        time.sleep(1)
        count = count - 1
    _, rv = api_ctx['app'](uri, headers={AUTH_HEADER: api_ctx['authadmin']})
    assert rv.status == 200
    r = api_ctx['images'].find_one({'_id': id})
    assert r['status'] == 'EXPIRED'


def test_metrics(api_ctx):
    rec = {
        "uid": 100,
        "user": "usera",
        "tag": api_ctx['tag'],
        "system": api_ctx['system'],
        "id": "fakeid",
        "type": "docker"
    }
    api_ctx['metrics'].remove({})
    for _ in range(100):
        rec['time'] = time.time()
        last_time = rec['time']
        api_ctx['metrics'].insert(rec.copy())
    uri = '%s/metrics/%s/?limit=20' % (api_ctx['url'], api_ctx['system'])
    _, rv = api_ctx['app'](uri, headers={AUTH_HEADER: api_ctx['authadmin']})
    assert rv.status == 200
    data = rv.json
    assert len(data) == 20
    assert data[19]['time'] == last_time


def test_import(api_ctx):
    api_ctx['config']["ImportUsers"] = "all"
    uri = '%s/doimport/%s/' % (api_ctx['url'], api_ctx['urlreq'])
    ifile = os.path.join(api_ctx['test_dir'], 'test.squashfs')
    data = {'filepath': ifile,
            'format': 'squashfs'}
    hash = fast_hash(ifile)
    datajson = json.dumps(data)
    _, rv = api_ctx['app'](uri, headers={AUTH_HEADER: api_ctx['auth']},
                       data=datajson)
    rv = time_wait(api_ctx['app'], api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'], op='doimport', data=datajson)
    data = rv.json
    assert data['status'] == 'READY'
    assert data['id'] == hash


def test_labels(api_ctx):
    from shifter_imagegw import api
    c = deepcopy(api_ctx['config'])
    c['Platforms']['systema']['use_external'] = True
    os.environ['ENABLE_LABELS'] = "1"
    api.mgr = ImageMngr(c)
    uri = '%s/pull/%s/' % (api_ctx['url'], api_ctx['urlreq'])
    app = api.app.test_client
    app.post(uri, headers={AUTH_HEADER: api_ctx['auth']})
    time.sleep(1)
    _, rv = app.post(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.json['status'] == 'READY'
    uri = '%s/lookup/%s/' % (api_ctx['url'], api_ctx['urlreq'])
    _, rv = app.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
    assert rv.status == 200
    resp = rv.json
    assert 'LABELS' in resp
    assert 'alabel' in resp['LABELS']
    assert resp['LABELS']['alabel'] == 'avalue'
