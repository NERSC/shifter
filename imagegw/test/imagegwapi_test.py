import os
import time
import urllib.request
import urllib.parse
import urllib.error
import json
from shifter_imagegw.fasthash import fast_hash
from pymongo import MongoClient
import pytest
from fastapi.testclient import TestClient
import warnings
warnings.filterwarnings('ignore', category=UserWarning, module='pymunge.raw') 
import shifter_imagegw.api as api
from shifter_imagegw.config import Config

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
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    monkeypatch.setenv("PATH", f"{test_dir}:{os.environ['PATH']}")


@pytest.fixture(autouse=True)
def set_gwconfig(monkeypatch):
    base_dir = os.path.dirname(os.path.abspath(__file__)) + "/.."
    cfg_test = os.path.abspath(os.path.join(base_dir, 'test.json'))
    cfg_example = os.path.abspath(os.path.join(base_dir, 'test.json.example'))
    configfile = cfg_test if os.path.exists(cfg_test) else cfg_example
    monkeypatch.setenv('GWCONFIG', configfile)
    monkeypatch.setenv('CONFIG', configfile)


@pytest.fixture()
def api_ctx():
    # Resolve config file, prefer test.json if present else fallback
    base_dir = os.path.dirname(os.path.abspath(__file__)) + "/.."
    cfg_test = os.path.abspath(os.path.join(base_dir, 'test.json'))
    cfg_example = os.path.abspath(os.path.join(base_dir, 'test.json.example'))
    configfile = cfg_test if os.path.exists(cfg_test) else cfg_example

    # Import after setting env so api reads correct config
    from shifter_imagegw import api

    # Prepare filesystem locations referenced by config
    with open(configfile) as f:
        data = json.load(f)
    config = Config(data)
    mongouri = config.MongoDBURI
    client = MongoClient(mongouri)
    dbname = config.MongoDB
    images = client[dbname].images
    metrics = client[dbname].metrics
    images.drop()
    metrics.delete_many({})

    if not os.path.exists(config.CacheDirectory):
        os.makedirs(config.CacheDirectory, exist_ok=True)
    p = config.Platforms['systema'].imageDir
    if not os.path.exists(p):
        os.makedirs(p, exist_ok=True)
    ctx = {
        'configfile': configfile,
        'config': config,
        'mgr': api.getmgr(),
        'api': api,
        'app': client,
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
    ctx['urlreq'] = "/".join([ctx['system'], ctx['itype'], ctx['tag']])

    yield ctx


def time_wait(app, url, auth, urlreq, data=None, state='READY', op='pull', TIMEOUT=10):
    poll_interval = 0.5
    count = TIMEOUT / poll_interval
    cstate = 'UNKNOWN'
    uri = '/'.join([url, op, urlreq])
    while (cstate != state and count > 0):
        if op == 'pull':
            response = app.post(uri, json=data, headers={AUTH_HEADER: auth})
        else:  # doimport
            response = app.post(uri, json=data, headers={AUTH_HEADER: auth})
        if response.status_code != 200:
            time.sleep(1)
            continue
        r = response.json()
        cstate = r['status']
        if r['status'] == 'READY':
            break
        if r['status'] == 'FAILURE':
            break
        if DEBUG:
            print(f'  {r["status"]}')
        time.sleep(1)
        count = count - 1
    return response


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


def test_pull1(api_ctx):
    with TestClient(api.app) as client:
        uri = f'{api_ctx["url"]}/pull/{api_ctx["urlreq"]}'
        data = {'allowed_uids': '1000,1001',
                'allowed_gids': '1002,1003'}
        response = client.post(uri, json=data, headers={AUTH_HEADER: api_ctx['auth']})
        data = response.json()
        assert 'userACL' in data
        assert 'groupACL' in data
        assert 1000 in data['userACL']
        assert 500 in data['userACL']
        assert 500 in data['groupACL']
        assert 1002 in data['groupACL']
        rv = time_wait(client, api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'])
        assert rv.status_code == 200


def test_list(api_ctx):
    with TestClient(api.app) as client:
        uri = "/".join([api_ctx["url"], 'list', 'systemc'])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 404
        rv = time_wait(client, api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'])
        assert rv.status_code == 200
        uri = "/".join([api_ctx["url"], 'list', api_ctx['system']])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200


def test_queue(api_ctx):
    with TestClient(api.app) as client:
        uri = "/".join([api_ctx["url"], 'pull', api_ctx['urlreq']])
        response = client.post(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200
        uri = "/".join([api_ctx["url"], 'queue', api_ctx['system']])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200


def test_pulllookup(api_ctx):
    with TestClient(api.app) as client:
        time_wait(client, api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'])
        uri = "/".join([api_ctx["url"], 'lookup', api_ctx['urlreq']])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200
        uri = "/".join([api_ctx["url"], 'lookup', api_ctx['system'], api_ctx['itype'], 'bogus'])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 404


def test_lookup(api_ctx):
    with TestClient(api.app) as client:
        record = good_record(api_ctx)
        id = api_ctx['images'].insert_one(record).inserted_id
        assert id is not None
        uri = "/".join([api_ctx["url"], 'lookup', api_ctx['urlreq']])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200


def test_expire(api_ctx):
    with TestClient(api.app) as client:
        uri = '/'.join([api_ctx['url'], 'expire', api_ctx['system'], api_ctx['itype'],
                                      api_ctx['tag']])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200


def test_autoexpire(api_ctx):
    with TestClient(api.app) as client:
        record = good_record(api_ctx)
        record['expiration'] = time.time() - 100
        id = api_ctx['images'].insert_one(record).inserted_id
        uri = '/'.join([api_ctx['url'], 'autoexpire', api_ctx['system']])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['authadmin']})
        assert response.status_code == 200

        count = 20
        while count > 0:
            response = client.get(uri, headers={AUTH_HEADER: api_ctx['authadmin']})
            r = api_ctx['images'].find_one({'_id': id})
            if r['status'] == 'EXPIRED':
                break
            time.sleep(1)
            count = count - 1
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['authadmin']})
        assert response.status_code == 200
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
    api_ctx['metrics'].delete_many({})
    for _ in range(100):
        rec['time'] = time.time()
        last_time = rec['time']
        api_ctx['metrics'].insert_one(rec.copy()).inserted_id
    with TestClient(api.app) as client:
        uri = f'{api_ctx["url"]}/metrics/{api_ctx["system"]}?limit=20'
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['authadmin']})
        assert response.status_code == 200
        data = response.json()
        assert len(data) == 20
        assert data[19]['time'] == last_time


def test_import(api_ctx):
    with TestClient(api.app) as client:
        api_ctx['config'].ImportUsers = "all"
        uri = '/'.join([api_ctx['url'], 'doimport', api_ctx['urlreq']])
        ifile = os.path.join(api_ctx['test_dir'], 'test.squashfs')
        data = {'filepath': ifile,
                'format': 'squashfs'}
        hash = fast_hash(ifile)
        response = client.post(uri, json=data, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200
        rv = time_wait(client, api_ctx['url'], api_ctx['auth'], api_ctx['urlreq'], op='doimport', data=data)
        data = rv.json()
        assert data['status'] == 'READY'
        assert data['id'] == hash


def test_labels(api_ctx):
    os.environ['ENABLE_LABELS'] = "1"
    with TestClient(api.app) as client:
        uri = f'{api_ctx["url"]}/pull/{api_ctx["urlreq"]}'
        client.post(uri, headers={AUTH_HEADER: api_ctx['auth']})
        time.sleep(1)
        response = client.post(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.json()['status'] == 'READY'
        uri = "/".join([api_ctx["url"], 'lookup', api_ctx['urlreq']])
        response = client.get(uri, headers={AUTH_HEADER: api_ctx['auth']})
        assert response.status_code == 200
        resp = response.json()
        assert 'LABELS' in resp
        assert 'alabel' in resp['LABELS']
        assert resp['LABELS']['alabel'] == 'avalue'
