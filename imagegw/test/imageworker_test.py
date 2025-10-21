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
from copy import deepcopy
from shifter_imagegw import imageworker
from shifter_imagegw.config import Config
DEBUG = False


def update_status(ident, state, meta=None):
    if DEBUG:
        print(f'id={ident} state={state}')


@pytest.fixture
def test_setup():
    """Setup test environment similar to unittest setUp"""
    cwd = os.path.dirname(os.path.realpath(__file__))
    os.environ['PATH'] = f'{cwd}:{os.environ["PATH"]}'
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

    return {
        'updater': updater,
        'config': config,
        'system': system,
        'itype': itype,
        'tag': tag,
        'request': request,
        'expandedpath': expandedpath,
        'imageDir': imageDir
    }


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


def test_pull_image_basic(test_setup):
    cleanup_cache(test_setup)
    request = {'system': test_setup['system'], 'itype': test_setup['itype'], 'tag': test_setup['tag']}
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    status = req._pull_image()
    assert status
    assert req.meta
    assert 'id' in req.meta
    assert 'workdir' in req.meta
    assert req.meta['entrypoint'][0] == "/bin/sh"
    assert req.meta['cmd'][0] == "/app.sh"
    assert os.path.exists(req.expandedpath)


# Pull the image but explicitly specify dockerhub
def test_pull_image_dockerhub(test_setup):
    request = {
        'system': test_setup['system'],
        'itype': test_setup['itype'],
        'tag': 'index.docker.io/ubuntu:latest'
    }
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    status = req._pull_image()
    assert status
    assert req.meta
    assert 'id' in req.meta
    assert os.path.exists(req.expandedpath)


# Use the URL format of the location, like an alias
def test_pull_image_url(test_setup):
    cleanup_cache(test_setup)
    request = {
        'system': test_setup['system'],
        'itype': test_setup['itype'],
        'tag': 'urltest/ubuntu:latest'
    }
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    status = req._pull_image()
    assert status
    assert req.meta
    assert 'id' in req.meta
    assert os.path.exists(req.expandedpath)


# Use the URL format of the location and pull a nested image
# (e.g. with an org)
def test_pull_image_url_org(test_setup):
    cleanup_cache(test_setup)
    request = {
        'system': test_setup['system'],
        'itype': test_setup['itype'],
        'tag': f'urltest/{test_setup["tag"]}'
    }
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    status = req._pull_image()
    assert status
    assert req.meta
    assert 'id' in req.meta
    assert os.path.exists(req.expandedpath)


def test_convert_image(test_setup):
    # Create a bogus tree
    cleanup_cache(test_setup)
    base = f"{test_setup['imageDir']}/image"
    if os.path.exists(base):
        shutil.rmtree(base)
    os.makedirs(f'{base}/a/b/c')
    request = test_setup['request']
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    req.id = 'bogus'
    req.expandedpath = base
    status = req._convert_image()
    assert status
    # Cleanup
    if os.path.exists(base):
        shutil.rmtree(base)


def test_transfer_image(test_setup):
    hash = 'bogus'
    imagefile = os.path.join(test_setup['config'].ExpandDirectory,
                             f'{hash}.squashfs')
    request = test_setup['request']
    request['imagefile'] = imagefile
    with open(imagefile, 'w') as f:
        f.write('bogus')
    assert os.path.exists(imagefile)
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    status = req._transfer_image()
    assert status


def test_bad_pull_docker(test_setup):
    cleanup_cache(test_setup)
    request = test_setup['request']
    conf = deepcopy(test_setup['config'])
    os.environ['UMOCI_FAIL'] = '1'
    req = imageworker.ImageRequest(conf, request, test_setup['updater'])
    with pytest.raises(OSError):
        req._pull_dockerv2('index.docker.io',
                           'scanon/shanetest', 'latest')
    os.environ.pop('UMOCI_FAIL')


def test_pull_docker(test_setup):
    cleanup_cache(test_setup)
    request = test_setup['request']
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    resp = req._pull_dockerv2('index.docker.io',
                              'scanon/shanetest', 'latest')
    assert resp

# def test_pull_docker_unicode(self):
#     request = {
#         'system': test_setup['system'],
#         'itype': test_setup['itype'],
#         'tag': 'index.docker.io/scanon/unicode:latest'
#     }
#     req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
#     status = req._pull_image()
#     assert status
#     assert req.meta
#     assert 'id' in req.meta
#     assert os.path.exists(req.expandedpath)
#     tfile = os.path.join(req.expandedpath, '\ua000')
#     assert os.path.exists(tfile)
#     return


def test_pull_image(test_setup):
    request = test_setup['request']
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    resp = req._pull_image()
    assert resp


def test_puller_real(test_setup):
    request = test_setup['request']
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    result = req.pull()
    mf = get_metafile(test_setup, result['id'])
    mfdata = read_metafile(mf)
    print(mfdata)
    assert 'WORKDIR' in mfdata
    request['userACL'] = [1001]
    req = imageworker.ImageRequest(test_setup['config'], request, test_setup['updater'])
    result = req.pull()
    req.remove_image()


def test_examine(test_setup):
    conf = deepcopy(test_setup['config'])
    conf.examiner = 'exam.sh'
    base = f"{test_setup['imageDir']}/image"
    request = {
               'system': test_setup['system'],
               }
    req = imageworker.ImageRequest(conf, request, test_setup['updater'])
    req.id = 'bogus'
    req.expandedpath = base
    result = req._examine_image()
    assert result
    req = imageworker.ImageRequest(conf, request, test_setup['updater'])
    req.id = 'bad'
    req.expandedpath = base
    result = req._examine_image()
    assert not result


def test_labels(test_setup):
    cleanup_cache(test_setup)
    request = test_setup['request']
    conf = deepcopy(test_setup['config'])
    req = imageworker.ImageRequest(conf, request, test_setup['updater'])
    resp = req.pull()
    assert 'labels' in resp
    assert 'alabel' in resp['labels']
