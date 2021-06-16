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
import unittest
import tempfile
from shifter_imagegw import transfer


class TransferTestCase(unittest.TestCase):
    system = {}
    inodes = 0

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

    def tearDown(self):
        """
        tear down should stop the worker
        """
        pass

    def test_sh_cmd(self):
        cmd = transfer._sh_cmd(self.system, 'echo', 'test')
        self.assertEqual(len(cmd), 2)
        self.assertEqual(cmd[0], 'echo')
        self.assertEqual(cmd[1], 'test')

        cmd = transfer._sh_cmd(self.system, 'anotherCommand')
        self.assertEqual(len(cmd), 1)
        self.assertEqual(cmd[0], 'anotherCommand')

        cmd = transfer._sh_cmd(self.system)
        self.assertIsNone(cmd)

    def test_ssh_cmd(self):
        cmd = transfer._ssh_cmd(self.system, 'echo', 'test')
        # expect ssh -i somefile nobody@localhost echo test
        self.assertEqual(len(cmd), 6)
        self.assertEqual('|'.join(cmd),
                          'ssh|-i|somefile|nobody@localhost|echo|test')

        self.system['ssh']['sshCmdOptions'] = ['-t']
        cmd = transfer._ssh_cmd(self.system, 'echo', 'test')
        self.assertEqual(len(cmd), 7)
        self.assertEqual('|'.join(cmd),
                          'ssh|-i|somefile|-t|nobody@localhost|echo|test')
        del self.system['ssh']['sshCmdOptions']

        cmd = transfer._ssh_cmd(self.system)
        self.assertIsNone(cmd)

    def test_cp_cmd(self):
        cmd = transfer._cp_cmd(self.system, 'a', 'b')
        self.assertEqual(len(cmd), 3)
        self.assertEqual('|'.join(cmd), 'cp|a|b')

    def test_scp_cmd(self):
        cmd = transfer._scp_cmd(self.system, 'a', 'b')
        self.assertEqual(len(cmd), 5)
        self.assertEqual('|'.join(cmd),
                          'scp|-i|somefile|a|nobody@localhost:b')

        self.system['ssh']['scpCmdOptions'] = ['-t']
        cmd = transfer._scp_cmd(self.system, 'a', 'b')
        self.assertEqual(len(cmd), 6)
        self.assertEqual('|'.join(cmd),
                          'scp|-i|somefile|-t|a|nobody@localhost:b')
        del self.system['ssh']['scpCmdOptions']

    def inode_counter(self, dirname):
        inodes = 0
        for root, dirs, files in os.walk(dirname):
            inodes += len(files)
        #self.inodes += len(fnames)
        return inodes

    def test_copyfile_local(self):
        tmp_path = tempfile.mkdtemp()
        self.system['local']['imageDir'] = tmp_path
        self.system['ssh']['imageDir'] = tmp_path

        self.system['accesstype'] = 'local'
        transfer.copy_file(__file__, self.system)
        fname = os.path.split(__file__)[1]
        file_path = os.path.join(tmp_path, fname)
        self.assertTrue(os.path.exists(file_path))

        self.inodes = 0
        inodes = self.inode_counter(tmp_path)
        self.assertEqual(inodes, 1)
        os.unlink(file_path)

        self.inodes = 0
        inodes = self.inode_counter(tmp_path)
        self.assertEqual(inodes, 0)

        os.rmdir(tmp_path)

    def test_copyfile_remote(self):
        """uses mock ssh/scp wrapper to pretend to do the remote
           transfer, ensure it is in PATH prior to running test
        """
        tmp_path = tempfile.mkdtemp()
        self.system['local']['imageDir'] = tmp_path
        self.system['ssh']['imageDir'] = tmp_path

        self.system['accesstype'] = 'remote'
        transfer.copy_file(__file__, self.system)
        fname = os.path.split(__file__)[1]
        file_path = os.path.join(tmp_path, fname)
        self.assertTrue(os.path.exists(file_path))

        self.inodes = 0
        inodes = self.inode_counter(tmp_path)
        self.assertEqual(inodes, 1)
        os.unlink(file_path)

        self.inodes = 0
        inodes = self.inode_counter(tmp_path)
        self.assertEqual(inodes, 0)

        os.rmdir(tmp_path)

    def test_copyfile_invalid(self):
        tmp_path = tempfile.mkdtemp()
        self.system['local']['imageDir'] = tmp_path
        self.system['ssh']['imageDir'] = tmp_path

        self.system['accesstype'] = 'invalid'

        with self.assertRaises(NotImplementedError):
            transfer.copy_file(__file__, self.system)

        os.rmdir(tmp_path)

    def test_copyfile_failtowrite(self):
        tmp_path = tempfile.mkdtemp()
        self.system['local']['imageDir'] = tmp_path
        self.system['ssh']['imageDir'] = tmp_path

        self.system['accesstype'] = 'local'

        # make directory unwritable
        os.chmod(tmp_path, 0o555)
        with self.assertRaises(OSError):
            transfer.copy_file(__file__, self.system)

        self.inodes = 0
        inodes = self.inode_counter(tmp_path)
        self.assertEqual(inodes, 0)
        os.rmdir(tmp_path)

    def test_transfer_local(self):
        tmp_path = tempfile.mkdtemp()
        self.system['local']['imageDir'] = tmp_path
        self.system['ssh']['imageDir'] = tmp_path

        self.system['accesstype'] = 'local'

        transfer.transfer(self.system, __file__)

        # make sure transferred file exists
        fname = os.path.split(__file__)[1]
        file_path = os.path.join(tmp_path, fname)
        self.assertTrue(os.path.exists(file_path))

        self.inodes = 0
        inodes = self.inode_counter(tmp_path)
        self.assertEqual(inodes, 1)
        os.unlink(file_path)

        dname, fname = os.path.split(__file__)
        meta = os.path.join(dname, '__init__.py')

        transfer.transfer(self.system, __file__, meta)
        self.inodes = 0
        inodes = self.inode_counter(tmp_path)
        self.assertEqual(inodes, 2)

        file_path = os.path.join(tmp_path, fname)
        meta_path = os.path.join(tmp_path, '__init__.py')

        os.unlink(file_path)
        os.unlink(meta_path)
        os.rmdir(tmp_path)

    def test_remove_local(self):
        (fdesc, tmp_path) = tempfile.mkstemp()
        os.close(fdesc)
        dname, fname = os.path.split(tmp_path)
        self.system['local']['imageDir'] = dname
        self.system['ssh']['imageDir'] = dname
        self.system['accesstype'] = 'local'

        transfer.remove_file(fname, self.system)
        self.assertEqual(os.path.exists(tmp_path), False)

    # TODO: Add a test_remove_remote

    def test_fasthash(self):
        (fdesc, tmp_path) = tempfile.mkstemp()
        os.close(fdesc)
        dname, fname = os.path.split(tmp_path)
        file = open(fname, 'w')
        file.write("blah blah hash")
        file.close()
        self.system['local']['imageDir'] = dname
        self.system['ssh']['imageDir'] = dname
        self.system['accesstype'] = 'local'

        hash = transfer.hash_file(fname, self.system)
        gh = 'ff68165577eb209adcfa2f793476a25da637142283409d6f4d8d61ee042c5e63'
        self.assertEqual(hash, gh)
        transfer.remove_file(fname, self.system)

    def test_get_stdout_and_log(self):
        cmd = "ls"
        stderr, stdout = transfer._get_stdout_and_log(cmd, None)
        self.assertIs(stderr, "")
        self.assertIsNotNone(stderr)
        self.assertIsNotNone(stdout)
        self.assertGreater(len(stdout), 0)

    def test_import_copy_file(self):
        (fdesc, tmp_path) = tempfile.mkstemp()
        os.close(fdesc)
        dname, fname = os.path.split(tmp_path)
        self.system['local']['imageDir'] = dname
        self.system['accesstype'] = 'local'
        cp_path = tmp_path+"_1"
        status = transfer.import_copy_file(tmp_path, cp_path,
                                           self.system, None)
        self.assertTrue(status)


if __name__ == '__main__':
    unittest.main()
