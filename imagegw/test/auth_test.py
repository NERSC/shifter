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

from asyncio import shield
import os
import pytest
import warnings
warnings.filterwarnings('ignore', category=UserWarning, module='pymunge.raw') 
from shifter_imagegw.auth import Authentication, authenticate
from shifter_imagegw.errors import AuthenticationError
from shifter_imagegw.config import Config
from shifter_imagegw.models import Session


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    monkeypatch.setenv("PATH", f"{os.environ['PATH']}:{test_dir}")


@pytest.fixture
def auth_env():
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    encoded = "xxxx\n"
    system = 'systema'
    data = {
        "Authentication": "munge",
        "Platforms": {system: {
                "mungeSocketPath": "/tmp/munge.s",
                "accesstype": "local",
                "local": {}
                }
        },
        "Locations": []
    }
    config = Config(data)

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
    resp = auth_env['auth'].authenticate(auth_env['encoded'],
                                         auth_env['system'])
    assert resp is not None
    assert isinstance(resp, Session)


def test_auth_replay(auth_env):
    resp = auth_env['auth'].authenticate(auth_env['encoded'],
                                         auth_env['system'])
    assert resp is not None
    with pytest.raises(AuthenticationError):
        auth_env['auth'].authenticate(auth_env['encoded'], auth_env['system'])


def test_auth_bad(auth_env):
    with pytest.raises(AuthenticationError):
        auth_env['auth'].authenticate("bad", auth_env['system'])


def test_authenticate(mocker):
    system = "systema"
    data = {
        "Authentication": "munge",
        "Platforms": {system: {
                "mungeSocketPath": "/tmp/munge.s",
                "accesstype": "local",
                "local": {}
                }
        },
        "Locations": []
    }
    c = Config(data)
    f = mocker.patch("shifter_imagegw.auth.decode")
    f.return_value = ('', 0, 0, object())
    sess = authenticate(c, "foo", system)
    assert sess.uid == 0
    assert sess.gid == 0
    assert sess.system == system
    assert sess.user == "root"
    assert sess.group == "wheel"

    f.return_value = ('', 700, 700, object())
    sess = authenticate(c, "foo", system)
    assert sess.uid == 700
    assert sess.gid == 700
    assert sess.system == system
    assert sess.user == "unknown"
    assert sess.group == "unknown"