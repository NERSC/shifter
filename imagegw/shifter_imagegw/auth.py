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

import json
from shifter_imagegw import munge

class Authentication(object):
    """
    Authentication Class to authenticate user requests
    """

    def __init__(self, config):
        """
        Initializes authenication handle.
        config is a dictionary.  It must define 'Authentication' and it must
        be a supported type (currently munge).
        Different auth mechanisms may require additional key value pairs
        """
        if 'Authentication' not in config:
            raise KeyError('Authentication not specified')
        self.sockets = dict()
        if config['Authentication'] == "munge":
            for system in config['Platforms']:
                self.sockets[system] = \
                        config['Platforms'][system]['mungeSocketPath']
            self.type = 'munge'
        elif config['Authentication'] == "mock":
            self.type = 'mock'
        else:
            memo = 'Unsupported auth type %s' % (config['Authentication'])
            raise NotImplementedError(memo)

    def _authenticate_munge(self, authstr, system=None):
        if self.type != 'munge':
            raise ValueError('incorrect authenticate type!')

        if authstr is None:
            raise KeyError("No Auth String Provided")
        if system is None:
            raise KeyError('System must be specified for munge')
        response = munge.unmunge(authstr, socket=self.sockets[system])
        if response is None:
            raise OSError('Authentication Failed')
        ret = dict()
        uids = response['UID']
        gids = response['GID']
        (user, uid) = uids.replace(' ', '').rstrip(')').split('(')
        (group, gid) = gids.replace(' ', '').rstrip(')').split('(')
        ret = {
            'user': user, 'uid': int(uid),
            'group': group, 'gid': int(gid),
            'tokens': ''
        }
        message_json = response['MESSAGE']
        try:
            ret['tokens'] = json.loads(message_json)['authorized_locations']
        except:
            pass
        return ret

    def _authenticate_mock(self, authstr, system=None):
        if self.type != 'mock':
            raise ValueError('incorrect authenticate type!')

        ret = dict()
        if authstr is None:
            raise KeyError("No Auth String Provided")
        auth = authstr.split(':')
        if len(auth) == 3:
            (status, user, group) = auth
            ret = {'user': user, 'group': group, 'tokens': ''}
        elif len(auth) == 4:
            (status, user, group, token) = auth
            ret = {'user': user, 'group': group, 'tokens': token}
        elif len(auth) == 6:
            (status, user, group, token, uid, gid) = auth
            ret = {'user': user, 'group': group, 'tokens': token,
                   'uid': int(uid), 'gid': int(gid)}
        else:
            raise OSError('Bad AuthString')

        if status != 'good':
            raise OSError('Auth Failed st=%s' % status)

        return ret

    def authenticate(self, authstr, system=None):
        """
        authenticate a message
        authstr is the message to be validated.
        system is required for munge.
        """
        if self.type == 'munge':
            return self._authenticate_munge(authstr, system)
        elif self.type == 'mock':
            return self._authenticate_mock(authstr, system)
        else:
            raise OSError('Unsupported auth type')
