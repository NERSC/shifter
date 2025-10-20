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
from shifter_imagegw import munge
from shifter_imagegw.errors import AuthenticationError


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    monkeypatch.setenv("PATH", f"{os.environ['PATH']}:{test_dir}")


@pytest.fixture(autouse=True)
def setup_files():
    test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
    encoded = "xxxx\n"
    message = "test"
    expired = "expired"
    with open(test_dir + "munge.test", 'w') as f:
        f.write(encoded)
    with open(test_dir + "munge.expired", 'w') as f:
        f.write('')
    return {"test_dir": test_dir, "encoded": encoded, "message": message, "expired": expired}


def test_munge(setup_files):
    resp = munge.munge(setup_files["message"])
    assert resp is not None


def test_unmunge(setup_files):
    resp = munge.unmunge(setup_files["encoded"])
    assert resp['MESSAGE'] == setup_files["message"]


def test_unmunge_expired(setup_files):
    with open(setup_files["test_dir"] + "munge.expired", 'w') as f:
        f.write(setup_files["expired"])
    with pytest.raises(AuthenticationError):
        munge.unmunge(setup_files["expired"])


def test_unmunge_replay(setup_files):
    resp = munge.unmunge(setup_files["encoded"])
    assert resp['MESSAGE'] == setup_files["message"]
    with pytest.raises(AuthenticationError):
        munge.unmunge(setup_files["encoded"])
