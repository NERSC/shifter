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
from shifter_imagegw.auth import Authentication

@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    monkeypatch.setenv("PATH", f"{os.environ['PATH']}:{test_dir}")


@pytest.fixture
def auth_env():
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    encoded = "xxxx\n"
    system = 'systema'
    config = {
        "Authentication": "munge",
        "Platforms": {system: {"mungeSocketPath": "/tmp/munge.s"}}
    }

    # setup
    with open(test_dir + "munge.test", 'w') as f:
        f.write(encoded)

    auth = Authentication(config)

    yield {
        'test_dir': test_dir,
        'encoded': encoded,
        'system': system,
        'auth': auth,
    }


def test_auth(auth_env):
    resp = auth_env['auth'].authenticate(auth_env['encoded'], auth_env['system'])
    assert resp is not None
    assert isinstance(resp, dict)


def test_auth_replay(auth_env):
    resp = auth_env['auth'].authenticate(auth_env['encoded'], auth_env['system'])
    assert resp is not None
    with pytest.raises(OSError):
        auth_env['auth'].authenticate(auth_env['encoded'], auth_env['system'])


def test_auth_bad(auth_env):
    with pytest.raises(OSError):
        auth_env['auth'].authenticate("bad", auth_env['system'])
