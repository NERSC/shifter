import os
import sys
import unittest
import tempfile

"""
Shifter, Copyright (c) 2016, The Regents of the University of California,
through Lawrence Berkeley National Laboratory (subject to receipt of any
required approvals from the U.S. Dept. of Energy).  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
 3. Neither the name of the University of California, Lawrence Berkeley
    National Laboratory, U.S. Dept. of Energy nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.`

See LICENSE for full text.
"""

from shifter_imagegw import transfer

class TransferTestCase(unittest.TestCase):
    system = {}

    def setUp(self):
        self.system['host'] = ['localhost']
        self.system['ssh'] = {
            'username': 'nobody',
            'key': 'somefile',
            'imageDir': None
        }
        self.system['local'] = {
            'imageDir': None
        }
        self.system['accesstype'] = 'local'
        pass

    def tearDown(self):
        """
        tear down should stop the worker
        """
        pass

    def test_shCmd(self):
        cmd = transfer._shCmd(self.system, 'echo', 'test')
        assert len(cmd) == 2
        assert cmd[0] == 'echo'
        assert cmd[1] == 'test'
     
        cmd = transfer._shCmd(self.system, 'anotherCommand')
        assert len(cmd) == 1
        assert cmd[0] == 'anotherCommand'

        cmd = transfer._shCmd(self.system)
        assert cmd is None

    def test_sshCmd(self):
        cmd = transfer._sshCmd(self.system, 'echo', 'test')
        ## expect ssh -i somefile nobody@localhost echo test
        assert len(cmd) == 6
        assert '|'.join(cmd) == 'ssh|-i|somefile|nobody@localhost|echo|test'

        self.system['ssh']['sshCmdOptions'] = ['-t']
        cmd = transfer._sshCmd(self.system, 'echo', 'test')
        assert len(cmd) == 7
        assert '|'.join(cmd) == 'ssh|-i|somefile|-t|nobody@localhost|echo|test'
        del self.system['ssh']['sshCmdOptions']

        cmd = transfer._sshCmd(self.system)
        assert cmd is None


    def test_cpCmd(self):
        cmd = transfer._cpCmd(self.system, 'a', 'b')
        assert len(cmd) == 3
        assert '|'.join(cmd) == 'cp|a|b'

    def test_scpCmd(self):
        cmd = transfer._scpCmd(self.system, 'a', 'b')
        assert len(cmd) == 5
        assert '|'.join(cmd) == 'scp|-i|somefile|a|nobody@localhost:b'

        self.system['ssh']['scpCmdOptions'] = ['-t']
        cmd = transfer._scpCmd(self.system, 'a', 'b')
        assert len(cmd) == 6
        assert '|'.join(cmd) == 'scp|-i|somefile|-t|a|nobody@localhost:b'
        del self.system['ssh']['scpCmdOptions']

    def inodeCounter(self, ignore, dirname, fnames):
        self.inodes += len(fnames)

    def test_copyfile_local(self):
        tmpPath = tempfile.mkdtemp() 
        self.system['local']['imageDir'] = tmpPath
        self.system['ssh']['imageDir'] = tmpPath
       
        self.system['accesstype'] = 'local'
        transfer.copy_file(__file__, self.system)
        dir,fname = os.path.split(__file__)
        filePath = os.path.join(tmpPath, fname)
        assert os.path.exists(filePath)

        self.inodes = 0
        os.path.walk(tmpPath, self.inodeCounter, None)
        assert self.inodes == 1
        os.unlink(filePath)

        self.inodes = 0
        os.path.walk(tmpPath, self.inodeCounter, None)
        assert self.inodes == 0

        os.rmdir(tmpPath)

    def test_copyfile_remote(self):
        """uses mock ssh/scp wrapper to pretend to do the remote
           transfer, ensure it is in PATH prior to running test
        """
        tmpPath = tempfile.mkdtemp() 
        self.system['local']['imageDir'] = tmpPath
        self.system['ssh']['imageDir'] = tmpPath
       
        self.system['accesstype'] = 'remote'
        transfer.copy_file(__file__, self.system)
        dir,fname = os.path.split(__file__)
        filePath = os.path.join(tmpPath, fname)
        assert os.path.exists(filePath)

        self.inodes = 0
        os.path.walk(tmpPath, self.inodeCounter, None)
        assert self.inodes == 1
        os.unlink(filePath)

        self.inodes = 0
        os.path.walk(tmpPath, self.inodeCounter, None)
        assert self.inodes == 0

        os.rmdir(tmpPath)

    def test_copyfile_invalid(self):
        tmpPath = tempfile.mkdtemp() 
        self.system['local']['imageDir'] = tmpPath
        self.system['ssh']['imageDir'] = tmpPath
       
        self.system['accesstype'] = 'invalid'
     
        with self.assertRaises(NotImplementedError):
            transfer.copy_file(__file__, self.system)

        os.rmdir(tmpPath)

    def test_copyfile_failtowrite(self):
        tmpPath = tempfile.mkdtemp() 
        self.system['local']['imageDir'] = tmpPath
        self.system['ssh']['imageDir'] = tmpPath
       
        self.system['accesstype'] = 'local'

        # make directory unwritable
        os.chmod(tmpPath, 0555)
        with self.assertRaises(OSError):
            transfer.copy_file(__file__, self.system)

        self.inodes = 0
        os.path.walk(tmpPath, self.inodeCounter, None)
        assert self.inodes == 0
        os.rmdir(tmpPath)

    def test_transfer_local(self):
        tmpPath = tempfile.mkdtemp() 
        self.system['local']['imageDir'] = tmpPath
        self.system['ssh']['imageDir'] = tmpPath
       
        self.system['accesstype'] = 'local'

        transfer.transfer(self.system, __file__)

        ## make sure transferred file exists
        dir,fname = os.path.split(__file__)
        filePath = os.path.join(tmpPath, fname)
        assert os.path.exists(filePath)

        self.inodes = 0
        os.path.walk(tmpPath, self.inodeCounter, None)
        assert self.inodes == 1
        os.unlink(filePath)

        dname,fname = os.path.split(__file__)
        meta = os.path.join(dname, '__init__.py')
        
        transfer.transfer(self.system, __file__, meta)
        self.inodes = 0
        os.path.walk(tmpPath, self.inodeCounter, None)
        assert self.inodes == 2

        filePath = os.path.join(tmpPath, fname)
        metaPath = os.path.join(tmpPath, '__init__.py')

        os.unlink(filePath)
        os.unlink(metaPath)
        os.rmdir(tmpPath)

    def test_remove_local(self):
        (fd,tmpPath) = tempfile.mkstemp() 
        dname,fname = os.path.split(tmpPath)
        self.system['local']['imageDir'] = dname
        self.system['ssh']['imageDir'] = dname
        self.system['accesstype'] = 'local'

        st=transfer.remove_file(fname, self.system)
        self.assertEquals(os.path.exists(tmpPath),False)

    # TODO: Add a test_remove_remote


if __name__ == '__main__':
    unittest.main()
