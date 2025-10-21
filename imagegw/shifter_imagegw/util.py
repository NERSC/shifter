#!/usr/bin/python

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

"""
  -------
  util.py
  -------

  Collection of functions that provide miscellaneous utilities

"""

import os
import stat
import shutil


def program_exists(program):
    """
    Checks if a program (bin) exists and raises an exception if not found.
    """
    if which(program) is None:
        raise IOError('Binary %s not found or not executable.' % str(program))
    return True


def which(program):
    """
    Sees if a program (bin) is executable and returns the path
    Borrowed from:
    http://stackoverflow.com/questions/377017/test-if-executable-exists-in-python
    """
    def is_exe(fpath):
        """
        checks if a given path exists and that the file is executable
        """
        return os.path.exists(fpath) and os.access(fpath, os.X_OK)

    def ext_candidates(fpath):
        """
        returns possible names for the executable based on the OS standards
        """
        yield fpath
        for ext in os.environ.get("PATHEXT", "").split(os.pathsep):
            yield fpath + ext

    fpath = os.path.split(program)[0]
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            exe_file = os.path.join(path, program)
            for candidate in ext_candidates(exe_file):
                if is_exe(candidate):
                    return candidate

    return None


def rmtree(path):
    def remove_readonly(func, path, _):
        "Clear the readonly bit and reattempt the removal"
        parent = os.path.dirname(path)
        os.chmod(parent, stat.S_IRWXU)
        if not os.path.islink(path):
            os.chmod(path, stat.S_IRWXU)
        func(path)

    shutil.rmtree(path, onerror=remove_readonly)
