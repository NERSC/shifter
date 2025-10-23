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


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    monkeypatch.setenv("PATH", f"{os.environ['PATH']}:{test_dir}")


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


def test_convert(converters_env):
    """
    Test convert function using a mock format
    """
    opts = dict()
    output = f'{converters_env["outdir"]}/test.squashfs'
    resp = converters.convert('mock', '', output)
    assert resp is True
    assert os.path.exists(output)
    os.remove(output)

    path = converters_env['make_fake']()

    resp = converters.convert('cramfs', path, '/tmp/blah.cramfs')
    assert resp

    with pytest.raises(NotImplementedError):
        converters.convert('ext4', path, '/tmp/blah.ext4')

    resp = converters.convert('squashfs', path, output, options=opts)
    assert resp
    with open(output) as f:
        line = f.read()
        assert '-no-xattrs' in line


# Disable this test
def xtest_convert_options(converters_env):
    """
    Test option handling
    """
    opts = {
        'mock': [' blah']
    }
    path = converters_env['make_fake']()
    output = f'{converters_env["outdir"]}/test.mock'
    if os.path.exists(output):
        os.remove(output)
    resp = converters.convert('mock', path, output, options=opts)
    assert resp
    with open(output) as f:
        v = f.read()
        assert v == 'bogus blah'

    os.remove(output)
    opts['mock'] = ' blah'
    resp = converters.convert('mock', path, output, options=opts)
    assert resp
    with open(output) as f:
        v = f.read()
        assert v == 'bogus blah'
    os.remove(output)


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
    keys = ['WORKDIR', 'FORMAT', 'ENTRY', 'CMD']
    if 'DISABLE_ACL_METADATA' not in os.environ:
        keys.extend(['USERACL', 'GROUPACL'])
    for key in keys:
        assert key in meta
    if 'DISABLE_ACL_METADATA' not in os.environ:
        assert meta['USERACL'].find("[") == -1
        assert meta['USERACL'].find("]") == -1
    assert len(meta['ENV']) > 0


def test_squashfs():
    converters.generate_squashfs_image('/tmp/b', '/tmp/blah', None)
    assert os.path.exists('/tmp/blah')
    os.remove('/tmp/blah')
