import os
import sys
import unittest
import tempfile

"""
Shifter, Copyright (c) 2015, The Regents of the University of California,
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

    def test_copyfile(self):
        tmpPath = tempfile.mkdtemp() 
        tmpPath = os.path.join(tmpPath, 'asdf')
        print tmpPath
        transfer.copy_file(self.system, __file__)


if __name__ == '__main__':
    unittest.main()
