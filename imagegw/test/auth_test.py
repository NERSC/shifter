import os
import unittest
import tempfile
import time
from shifter_imagegw import auth
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

class AuthTestCase(unittest.TestCase):

    def setUp(self):
        #os.environ['PATH']=os.environ['PATH']+":./test"
        import munge
        self.encoded="xxxx\n"
        self.message="test"
        self.expired="expired"
        with open("./test/munge.test",'w') as f:
          f.write(self.encoded)
        self.system='systema'
        self.config={"Authentication":"munge",
            "Platforms":{self.system:{"mungeSocketPath": "/tmp/munge.s"}}}
        self.auth=auth.authentication(self.config)

    def tearDown(self):
        with open("./test/munge.expired",'w') as f:
            f.write('')

    def test_auth(self):
        """ Test success """
        try:
          resp=self.auth.authenticate(self.encoded,self.system)
        except:
          assert False
        assert resp is not None
        assert len(resp)==3

    def test_auth_replay(self):
        try:
          resp=self.auth.authenticate(self.encoded,self.system)
        except:
          assert False
        assert resp is not None
        try:
          resp=self.auth.authenticate(self.encoded,self.system)
          assert False
        except:
          assert True

    def test_auth_bad(self):
        try:
          # This should raise an error
          self.auth.authenticate("bad",self.system)
          assert False
        except:
          assert True


if __name__ == '__main__':
    unittest.main()
