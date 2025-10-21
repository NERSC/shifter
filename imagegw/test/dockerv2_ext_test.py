# Shifter, Copyright (c) 2016, The Regents of the University of California,
# through Lawrence Berkeley National Laboratory (subject to receipt of any
# required approvals from the U.S. Dept. of Energy).  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. Neither the name of the University of California, Lawrence Berkeley
#     National Laboratory, U.S. Dept. of Energy nor the names of its
#     contributors may be used to endorse or promote products derived from this
#     software without specific prior written permission.`
#
# See LICENSE for full text.

import os
from shifter_imagegw.dockerv2_ext import DockerV2ext
from shifter_imagegw.util import rmtree
import tempfile
import json
import base64
import pytest


class update():

    def update_status(self, state, message):
        print(f"{state}: {message}")


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    cwd = os.path.dirname(os.path.realpath(__file__))
    monkeypatch.setenv('PATH', cwd + ':' + os.environ['PATH'])


@pytest.fixture
def test_dir():
    return os.path.dirname(os.path.realpath(__file__)) + '/'


@pytest.fixture
def updater():
    return update()


@pytest.fixture
def tokens(test_dir):
    cfg = os.path.join(test_dir, 'tokens.cfg')
    if not os.path.exists(cfg):
        return None
    with open(cfg) as f:
        d = json.load(f)
    return d


def test_pull_public(updater):
    cache = tempfile.mkdtemp()
    expand = tempfile.mkdtemp()
    image = 'alpine'
    try:
        dock = DockerV2ext(image, cachedir=cache, updater=updater)
        resp = dock.examine_manifest()
        assert 'id' in resp
        assert 'private' in resp
        assert 'labels' in resp
        resp = dock.pull_layers()
        assert resp
        dock.extract_docker_layers(expand)
        assert os.path.exists(os.path.join(expand, "bin"))
    finally:
        rmtree(expand)
        rmtree(cache)


def test_permission_bug(updater):
    cache = tempfile.mkdtemp()
    expand = tempfile.mkdtemp()
    image = 'scanon/permtest'
    try:
        dock = DockerV2ext(image, cachedir=cache, updater=updater)
        resp = dock.examine_manifest()
        assert 'id' in resp
        assert 'private' in resp
        resp = dock.pull_layers()
        assert resp
        dock.extract_docker_layers(expand)
        assert os.path.exists(os.path.join(expand, "bin"))
    finally:
        rmtree(expand)
        rmtree(cache)


def test_pull_private(tokens, updater):
    if tokens is None:
        pytest.skip("no tokens.cfg available")
    tok = base64.b64decode(tokens['index.docker.io']).decode('utf-8')
    username, password = tok.split(':')
    image = 'scanon/shaneprivate'
    options = {'username': username, 'password': password}
    cache = tempfile.mkdtemp()
    expand = tempfile.mkdtemp()
    try:
        dock = DockerV2ext(image, options=options, cachedir=cache,
                           updater=updater)
        resp = dock.examine_manifest()
        assert resp['private']
        resp = dock.pull_layers()
        assert resp
        dock.extract_docker_layers(expand)
        assert os.path.exists(os.path.join(expand, 'bin'))
    finally:
        rmtree(expand)
        rmtree(cache)


def test_pull_private2(tokens, updater):
    if tokens is None:
        pytest.skip("no tokens.cfg available")
    tok = base64.b64decode(tokens['registry.services.nersc.gov']).decode('utf-8')
    username, password = tok.split(':')
    image = 'scanon/alpine'
    options = {'baseUrl': 'https://registry.services.nersc.gov',
               'username': username, 'password': password}
    cache = tempfile.mkdtemp()
    expand = tempfile.mkdtemp()
    try:
        dock = DockerV2ext(image, options=options, cachedir=cache,
                           updater=updater)
        resp = dock.examine_manifest()
        assert resp['private']
        resp = dock.pull_layers()
        assert resp
        dock.extract_docker_layers(expand)
        assert os.path.exists(os.path.join(expand, 'bin'))
    finally:
        rmtree(expand)
        rmtree(cache)


def test_base_url():
    image = 'scanon/alpine'
    opt = {'baseUrl': 'https://foo.bar/'}
    dock = DockerV2ext(image, options=opt)
    assert dock.registry == 'foo.bar'


def test_authfile(tmp_path):
    image = 'alpine'
    dock = DockerV2ext(image)
    with pytest.raises(OSError):
        dock._auth_file()
    opt = {'username': 'foo', 'password': 'bar'}
    dock = DockerV2ext(image, options=opt)
    dock._auth_file()
    fn = str(dock.auth_file)
    with open(fn) as f:
        data = f.read()
    js = json.loads(data)
    assert 'auths' in js
    os.unlink(fn)


def test_private(test_dir):
    image = 'private'
    dock = DockerV2ext(image)
    with pytest.raises(OSError):
        dock._auth_file()
    opt = {'username': 'foo',
           'password': 'bar',
           'policy_file': test_dir + 'policy.json'
           }
    dock = DockerV2ext(image, options=opt)
    dock._auth_file()
    fn = str(dock.auth_file)
    with open(fn) as f:
        data = f.read()
    js = json.loads(data)
    assert 'auths' in js


def test_policyfile(test_dir):
    image = 'alpine'
    pf = test_dir + 'policy.json'
    opt = {'policy_file': pf}
    dock = DockerV2ext(image, options=opt)
    assert pf == dock.policy_file
