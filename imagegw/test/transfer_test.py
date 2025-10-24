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
import tempfile
import pytest
from shifter_imagegw import transfer
from shifter_imagegw.config import Platform


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    monkeypatch.setenv("PATH", f"{test_dir}:{os.environ['PATH']}")


@pytest.fixture
def system():
    return Platform({})


def inode_counter(dirname):
    inodes = 0
    for root, dirs, files in os.walk(dirname):
        inodes += len(files)
    return inodes


def test_transfer_local(system):
    tmp_path = tempfile.mkdtemp()
    system.imageDir = tmp_path

    system.accesstype = "local"

    transfer.transfer(system, __file__)

    # make sure transferred file exists
    fname = os.path.split(__file__)[1]
    file_path = os.path.join(tmp_path, fname)
    assert os.path.exists(file_path)

    inodes = inode_counter(tmp_path)
    assert inodes == 1
    os.unlink(file_path)

    dname, fname = os.path.split(__file__)
    meta = os.path.join(dname, '__init__.py')

    transfer.transfer(system, __file__, meta)
    inodes = inode_counter(tmp_path)
    assert inodes == 2

    file_path = os.path.join(tmp_path, fname)
    meta_path = os.path.join(tmp_path, '__init__.py')

    os.unlink(file_path)
    os.unlink(meta_path)
    os.rmdir(tmp_path)


def test_remove(system):
    (fdesc, tmp_path) = tempfile.mkstemp()
    os.close(fdesc)
    dname, fname = os.path.split(tmp_path)  

    (fdesc2, tmp_path2) = tempfile.mkstemp()
    os.close(fdesc2)
    dname, fname2 = os.path.split(tmp_path2)

    system.imageDir = dname
    system.accesstype = "local"

    status = transfer.remove(system, fname, fname2)

    assert status
    # transfer.remove_file(fname, system)
    assert os.path.exists(tmp_path) is False
    assert os.path.exists(tmp_path2) is False


def test_fasthash(system):
    (fdesc, tmp_path) = tempfile.mkstemp()
    os.close(fdesc)
    dname, fname = os.path.split(tmp_path)
    file = open(fname, 'w')
    file.write("blah blah hash")
    file.close()
    system.imageDir = dname
    system.accesstype = "local"

    hash = transfer.hash_file(fname, system)
    gh = 'ff68165577eb209adcfa2f793476a25da637142283409d6f4d8d61ee042c5e63'
    assert hash == gh
    transfer.remove_file(fname, system)

def test_check_file(system):
    fn = "foo"
    status = transfer.check_file(fn, system)
    assert not status

    (fdesc, tmp_path) = tempfile.mkstemp()
    os.close(fdesc)
    dname, fname = os.path.split(tmp_path)
    system.imageDir = dname

    status = transfer.check_file(fname, system)
    assert status


def test_imgvalid(system):
    fn = "foo"
    status = transfer.check_file(fn, system)
    assert not status

    (fdesc, tmp_path) = tempfile.mkstemp()
    os.close(fdesc)
    dname, fname = os.path.split(tmp_path)
    system.imageDir = dname

    (fdesc2, tmp_path2) = tempfile.mkstemp()
    os.close(fdesc2)
    _, fname2 = os.path.split(tmp_path2)

    status = transfer.imagevalid(system, fname, fname2)
    assert status
    os.unlink(tmp_path)
    os.unlink(tmp_path2)