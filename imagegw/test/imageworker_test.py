# Shifter, Copyright (c) 2015, The Regents of the University of California,
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
import pytest
import json
import shutil
from shifter_imagegw.config import Config
from shifter_imagegw.models import Session
from shifter_imagegw.imageworker import PullRequest
from shifter_imagegw import imageworker
DEBUG = False


def update_status(ident, state, meta=None):
    if DEBUG:
        print(f'id={ident} state={state}')


@pytest.fixture
def test_setup():
    """Setup test environment similar to unittest setUp"""
    cwd = os.path.dirname(os.path.realpath(__file__))
    os.environ['PATH'] = f'{cwd}/fakebin:{os.environ["PATH"]}'
    updater = imageworker.Updater('bogusid', update_status)
    configfile = 'test.json'
    with open(configfile) as config_file:
        data = json.load(config_file)
    config = Config(data=data)

    system = 'systema'
    itype = 'docker'
    tag = 'registry.services.nersc.gov/nersc-py:latest'
    tag = 'ubuntu:latest'
    tag = 'scanon/shanetest:latest'
    request = {
        'system': system,
        'itype': itype,
        'tag': tag
    }
    if 'LOCALREGISTRY' in os.environ:
        config.DefaultImageLocation = 'local'
        tag = f'local/{tag}'
    if not os.path.exists(config.CacheDirectory):
        os.mkdir(config.CacheDirectory)
    expandedpath = \
        os.path.join(config.CacheDirectory,
                     f'{itype}_{tag.replace("/", "_")}')
    idir = config.Platforms['systema'].imageDir
    if not os.path.exists(idir):
        os.makedirs(idir)
    imageDir = idir
    session = Session(uid=100, gid=100,
                      user="user", group="user",
                      system=system)

    return {
        'updater': updater,
        'config': config,
        'system': system,
        'itype': itype,
        'tag': tag,
        'request': request,
        'expandedpath': expandedpath,
        'imageDir': imageDir,
        'session': session
    }


@pytest.fixture
def pull_request(test_setup):
    ctx = test_setup
    cleanup_cache(ctx)
    req = PullRequest(test_setup['config'],
                      ctx['system'],
                      ctx['tag'],
                      'foo',
                      ctx['session'])
    return req


def cleanup_cache(test_setup):
    """Helper function to clean up cache"""
    paths = [test_setup['imageDir'], test_setup['config'].CacheDirectory]
    for path in paths:
        for f in os.listdir(path):
            if f.find('.squashfs') > 0 or f.find('.meta') > 0:
                fn = os.path.join(path, f)
                os.remove(fn)


def get_metafile(test_setup, id):
    """Helper function to get metafile path"""
    metafile = os.path.join(test_setup['imageDir'], f'{id}.meta')
    return metafile


def read_metafile(metafile):
    """Helper function to read metafile"""
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


def test_pull_image_basic(pull_request):
    status = pull_request._pull_image()
    assert status
    assert pull_request.meta
    assert 'id' in pull_request.meta
    assert 'workdir' in pull_request.meta
    assert pull_request.meta['entrypoint'][0] == "/bin/sh"
    assert pull_request.meta['cmd'][0] == "/app.sh"
    assert os.path.exists(pull_request.expandedpath)


# Pull the image but explicitly specify dockerhub
def test_pull_image_dockerhub(pull_request):
    pull_request.tag = 'index.docker.io/ubuntu:latest'
    status = pull_request._pull_image()
    assert status
    assert pull_request.meta
    assert 'id' in pull_request.meta
    assert os.path.exists(pull_request.expandedpath)


# Use the URL format of the location, like an alias
def test_pull_image_url(pull_request):
    pull_request.tag = 'urltest/ubuntu:latest'
    status = pull_request._pull_image()
    assert status
    assert pull_request.meta
    assert 'id' in pull_request.meta
    assert os.path.exists(pull_request.expandedpath)


# Use the URL format of the location and pull a nested image
# (e.g. with an org)
def test_pull_image_url_org(test_setup, pull_request):
    pull_request.tag = f'urltest/{test_setup["tag"]}'
    status = pull_request._pull_image()
    assert status
    assert pull_request.meta
    assert 'id' in pull_request.meta
    assert os.path.exists(pull_request.expandedpath)


def test_convert_image(test_setup, pull_request):
    # Create a bogus tree
    # cleanup_cache(test_setup)
    base = f"{test_setup['imageDir']}/image"
    if os.path.exists(base):
        shutil.rmtree(base)
    os.makedirs(f'{base}/a/b/c')
    pull_request.id = 'bogus'
    pull_request.expandedpath = base
    status = pull_request._convert_image()
    assert status
    # Cleanup
    if os.path.exists(base):
        shutil.rmtree(base)


def test_transfer_image(test_setup, pull_request):
    hash = 'bogus'
    imagefile = os.path.join(test_setup['config'].ExpandDirectory,
                             f'{hash}.squashfs')
    pull_request.imagefile = imagefile
    with open(imagefile, 'w') as f:
        f.write('bogus')
    assert os.path.exists(imagefile)
    pull_request._transfer_image()


def test_bad_pull_docker(test_setup, pull_request):
    os.environ['UMOCI_FAIL'] = '1'
    with pytest.raises(OSError):
        pull_request._pull_dockerv2('index.docker.io',
                                    'scanon/shanetest', 'latest')
    os.environ.pop('UMOCI_FAIL')


def test_pull_docker(pull_request):
    resp = pull_request._pull_dockerv2('index.docker.io',
                                       'scanon/shanetest', 'latest')
    assert resp


def test_pull_image(pull_request):
    resp = pull_request._pull_image()
    assert resp


def test_puller_real(test_setup, pull_request):
    result = pull_request.run()
    mf = get_metafile(test_setup, result['id'])
    mfdata = read_metafile(mf)

    assert 'WORKDIR' in mfdata
    pull_request.userACL = [1001]
    result = pull_request.run()


def test_examine(test_setup, pull_request):
    base = f"{test_setup['imageDir']}/image"
    pull_request.conf.examiner = "exam.sh"
    pull_request.id = 'bogus'
    pull_request.expandedpath = base
    result = pull_request._examine_image()
    assert result
    pull_request.id = 'bad'
    pull_request.expandedpath = base
    result = pull_request._examine_image()
    assert not result


def test_labels(pull_request):
    resp = pull_request.run()
    assert 'labels' in resp
    assert 'alabel' in resp['labels']


def test_pull_image_new(test_setup):
    cleanup_cache(test_setup)
    system = test_setup['system']
    req = imageworker.PullRequest(test_setup['config'],
                                  test_setup['system'],
                                  test_setup['tag'],
                                  "foo",
                                  test_setup['session'])
    status = req.run()
    assert status
    assert req.meta
    assert 'id' in req.meta
    assert 'workdir' in req.meta
    assert req.meta['entrypoint'][0] == "/bin/sh"
    assert req.meta['cmd'][0] == "/app.sh"
    id = "468b48e3864f5489a6fa4a35843292b101ac73c31e3272688fa3220ff485f549"
    fn = f"{test_setup['config'].Platforms[system].imageDir}/{id}.squashfs"
    assert os.path.exists(fn)
