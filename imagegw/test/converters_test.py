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
from shifter_imagegw import converters
from shifter_imagegw.util import program_exists


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    monkeypatch.setenv("PATH", f"{test_dir}/fakebin:{os.environ['PATH']}")


@pytest.fixture
def converters_env():
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    if 'TMPDIR' in os.environ:
        outdir = os.environ['TMPDIR']
    else:
        outdir = '/tmp/'

    def make_fake():
        path = outdir + "/fakeimage"
        if os.path.exists(path) is False:
            os.makedirs(path)
        with open(path + '/a', "w") as f:
            f.write('blah')
        return path

    return {
        'test_dir': test_dir,
        'outdir': outdir,
        'make_fake': make_fake,
    }


def test_unsupported(converters_env):
    """
    Test convert function using a mock format
    """

    with pytest.raises(NotImplementedError):
        converters.convert('cramfs', None, '/tmp/blah.cramfs')

    with pytest.raises(NotImplementedError):
        converters.convert('ext4', None, '/tmp/blah.ext4')


def test_failure():
    out = '/tmp/fakeout'
    if os.path.exists(out):
        os.unlink(out)
    with pytest.raises(OSError):
        converters.convert('squashfs', '/tmp/foo', out, options='fail')

    with pytest.raises(FileExistsError):
        converters.convert('squashfs', '/tmp/foo', '/', options='fail')

    with pytest.raises(IOError):
        program_exists('/bin/shouldnotexist')


def test_convert_options(converters_env):
    """
    Test option handling
    """
    opts = ['-fake-opt']

    path = converters_env['make_fake']()
    output = f'{converters_env["outdir"]}/test.mock'
    if os.path.exists(output):
        os.remove(output)
    resp = converters.convert('squashfs', path, output, options=opts)
    assert resp
    with open(output) as f:
        v = f.read()
        assert opts[0] in v
    os.remove(output)

    resp = converters.convert('squashfs', path, output, options=opts[0])
    assert resp
    with open(output) as f:
        v = f.read()
        assert opts[0] in v
    os.remove(output)

    with pytest.raises(ValueError):
        resp = converters.convert('squashfs', path, output,
                                  options={'foo': 'bar'})


def test_writemeta(converters_env):
    """
    Test Write meta function
    """

    meta = {'workdir': "/tmp/",
            'entrypoint': '/bin/sh',
            'cmd': '/script',
            'env': ['a=b', 'c=d'],
            'private': True,
            'userACL': [1000, 1001],
            'groupACL': [1002, 1003],
            'user': 'user'
            }
    output = f'{converters_env["outdir"]}/test.meta'
    resp = converters.writemeta('squashfs', meta, output)
    assert resp is not None
    meta = {'ENV': []}
    with open(output) as f:
        for line in f:
            (k, v) = line.strip().split(": ", 2)
            if k == 'ENV':
                meta['ENV'].append(v)
            else:
                meta[k] = v
    keys = ['WORKDIR', 'FORMAT', 'ENTRY', 'CMD', 'USER']
    keys.extend(['USERACL', 'GROUPACL'])
    for key in keys:
        assert key in meta
    assert meta['USERACL'] == '1000,1001'
    assert meta['GROUPACL'] == '1002,1003'
    assert len(meta['ENV']) > 0


def test_squashfs(set_path, converters_env):
    path = converters_env['make_fake']()
    output = f'{converters_env["outdir"]}/test.squashfs'
    resp = converters.convert('squashfs', path, output)
    assert resp
    with open(output) as f:
        line = f.read()
        assert '-no-xattrs' in line
    os.remove(output)
