#!/usr/bin/env python
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
Module to abstract authentication.  Currently just wraps munge.
"""

from shifter_imagegw.config import Config
from pymunge import decode, MungeError
from shifter_imagegw.models import Session
from shifter_imagegw.errors import AuthenticationError
import pwd
import grp


def authenticate(conf: Config, authstr: str, system: str):
    """
    Authenticate a user using a munge token.

    Throws AuthenticationError on a failure
    """

    if conf.Authentication != 'munge':
        raise NotImplementedError(f'{conf.Authentication} is not supported')
    try:
        token, uid, gid, _ = decode(authstr)
    except MungeError as e:
        raise AuthenticationError(e)
    user = "unknown"
    group = "unknown"

    # Ignore lookup errors for now
    try:
        user = pwd.getpwuid(uid).pw_name
    except KeyError:
        pass
    try:
        group = grp.getgrgid(gid).gr_name
    except KeyError:
        pass
    return Session(uid=uid, gid=gid, tokens=token,
                   system=system, user=user, group=group)
