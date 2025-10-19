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
import sys
from shifter_imagegw import transfer


@pytest.fixture(autouse=True)
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    monkeypatch.setenv("PATH", f"{test_dir}:{os.environ['PATH']}")


@pytest.fixture
def system():
    sys_cfg = {}
    sys_cfg['host'] = ['localhost']
    sys_cfg['ssh'] = {
        'username': 'nobody',
        'key': 'somefile',
        'imageDir': None
    }
    sys_cfg['local'] = {
        'imageDir': None
    }
    sys_cfg['accesstype'] = 'local'
    return sys_cfg


def inode_counter(dirname):
    inodes = 0
    for root, dirs, files in os.walk(dirname):
        inodes += len(files)
    return inodes


def test_sh_cmd(system):
    cmd = transfer._sh_cmd(system, 'echo', 'test')
    assert len(cmd) == 2
    assert cmd[0] == 'echo'
    assert cmd[1] == 'test'

    cmd = transfer._sh_cmd(system, 'anotherCommand')
    assert len(cmd) == 1
    assert cmd[0] == 'anotherCommand'

    cmd = transfer._sh_cmd(system)
    assert cmd is None


def test_ssh_cmd(system):
    cmd = transfer._ssh_cmd(system, 'echo', 'test')
    # expect ssh -i somefile nobody@localhost echo test
    assert len(cmd) == 6
    assert '|'.join(cmd) == 'ssh|-i|somefile|nobody@localhost|echo|test'

    system['ssh']['sshCmdOptions'] = ['-t']
    cmd = transfer._ssh_cmd(system, 'echo', 'test')
    assert len(cmd) == 7
    assert '|'.join(cmd) == 'ssh|-i|somefile|-t|nobody@localhost|echo|test'
    del system['ssh']['sshCmdOptions']

    cmd = transfer._ssh_cmd(system)
    assert cmd is None


def test_cp_cmd(system):
    cmd = transfer._cp_cmd(system, 'a', 'b')
    assert len(cmd) == 3
    assert '|'.join(cmd) == 'cp|a|b'


def test_scp_cmd(system):
    cmd = transfer._scp_cmd(system, 'a', 'b')
    assert len(cmd) == 5
    assert '|'.join(cmd) == 'scp|-i|somefile|a|nobody@localhost:b'

    system['ssh']['scpCmdOptions'] = ['-t']
    cmd = transfer._scp_cmd(system, 'a', 'b')
    assert len(cmd) == 6
    assert '|'.join(cmd) == 'scp|-i|somefile|-t|a|nobody@localhost:b'
    del system['ssh']['scpCmdOptions']


def test_copyfile_local(system):
    tmp_path = tempfile.mkdtemp()
    system['local']['imageDir'] = tmp_path
    system['ssh']['imageDir'] = tmp_path

    system['accesstype'] = 'local'
    transfer.copy_file(__file__, system)
    fname = os.path.split(__file__)[1]
    file_path = os.path.join(tmp_path, fname)
    assert os.path.exists(file_path)

    inodes = inode_counter(tmp_path)
    assert inodes == 1
    os.unlink(file_path)

    inodes = inode_counter(tmp_path)
    assert inodes == 0

    os.rmdir(tmp_path)


def test_copyfile_remote(system):
    """uses mock ssh/scp wrapper to pretend to do the remote
       transfer, ensure it is in PATH prior to running test
    """
    tmp_path = tempfile.mkdtemp()
    system['local']['imageDir'] = tmp_path
    system['ssh']['imageDir'] = tmp_path

    system['accesstype'] = 'remote'
    transfer.copy_file(__file__, system)
    fname = os.path.split(__file__)[1]
    file_path = os.path.join(tmp_path, fname)
    assert os.path.exists(file_path)

    inodes = inode_counter(tmp_path)
    assert inodes == 1
    os.unlink(file_path)

    inodes = inode_counter(tmp_path)
    assert inodes == 0

    os.rmdir(tmp_path)


def test_copyfile_invalid(system):
    tmp_path = tempfile.mkdtemp()
    system['local']['imageDir'] = tmp_path
    system['ssh']['imageDir'] = tmp_path

    system['accesstype'] = 'invalid'

    with pytest.raises(NotImplementedError):
        transfer.copy_file(__file__, system)

    os.rmdir(tmp_path)


def test_copyfile_failtowrite(system):
    tmp_path = tempfile.mkdtemp()
    system['local']['imageDir'] = tmp_path
    system['ssh']['imageDir'] = tmp_path

    system['accesstype'] = 'local'

    # make directory unwritable
    os.chmod(tmp_path, 0o555)
    with pytest.raises(OSError):
        transfer.copy_file(__file__, system)

    inodes = inode_counter(tmp_path)
    assert inodes == 0
    os.rmdir(tmp_path)


def test_transfer_local(system):
    tmp_path = tempfile.mkdtemp()
    system['local']['imageDir'] = tmp_path
    system['ssh']['imageDir'] = tmp_path

    system['accesstype'] = 'local'

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


def test_remove_local(system):
    (fdesc, tmp_path) = tempfile.mkstemp()
    os.close(fdesc)
    dname, fname = os.path.split(tmp_path)
    system['local']['imageDir'] = dname
    system['ssh']['imageDir'] = dname
    system['accesstype'] = 'local'

    transfer.remove_file(fname, system)
    assert os.path.exists(tmp_path) is False


def test_fasthash(system):
    (fdesc, tmp_path) = tempfile.mkstemp()
    os.close(fdesc)
    dname, fname = os.path.split(tmp_path)
    file = open(fname, 'w')
    file.write("blah blah hash")
    file.close()
    system['local']['imageDir'] = dname
    system['ssh']['imageDir'] = dname
    system['accesstype'] = 'local'

    hash = transfer.hash_file(fname, system)
    gh = 'ff68165577eb209adcfa2f793476a25da637142283409d6f4d8d61ee042c5e63'
    assert hash == gh
    transfer.remove_file(fname, system)


def test_get_stdout_and_log():
    cmd = "ls"
    stderr, stdout = transfer._get_stdout_and_log(cmd, None)
    assert stderr == ""
    assert stderr is not None
    assert stdout is not None
    assert len(stdout) > 0


def test_import_copy_file(system):
    (fdesc, tmp_path) = tempfile.mkstemp()
    os.close(fdesc)
    dname, fname = os.path.split(tmp_path)
    system['local']['imageDir'] = dname
    system['accesstype'] = 'local'
    cp_path = tmp_path+"_1"
    status = transfer.import_copy_file(tmp_path, cp_path,
                                       system, None)
    assert status
