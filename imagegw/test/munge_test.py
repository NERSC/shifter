import os
import unittest
import tempfile
import time
from shifter_imagegw import munge
import json

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

class MungeTestCase(unittest.TestCase):

    def setUp(self):
        #os.environ['PATH']=os.environ['PATH']+":./test"
        self.test_dir=os.path.dirname(os.path.abspath(__file__))+"/../test/"
        self.encoded="xxxx\n"
        self.message="test"
        self.expired="expired"
        with open(self.test_dir+"munge.test",'w') as f:
          f.write(self.encoded)

    def tearDown(self):
        with open(self.test_dir+"munge.expired",'w') as f:
            f.write('')

    def test_munge(self):
        resp=munge.munge(self.message)
        assert resp is not None

    def test_unmunge(self):
        resp=munge.unmunge(self.encoded)
        assert resp['MESSAGE']==self.message

    def test_unmunge_expired(self):
        with open(self.test_dir+"munge.expired",'w') as f:
            f.write(self.expired)
        try:
            resp=munge.unmunge(self.expired)
            assert False
        except OSError:
            assert True

    def test_unmunge_replay(self):
        resp=munge.unmunge(self.encoded)
        assert resp['MESSAGE']==self.message
        try:
          resp=munge.unmunge(self.encoded)
          assert False
        except OSError:
          assert True

if __name__ == '__main__':
    unittest.main()
