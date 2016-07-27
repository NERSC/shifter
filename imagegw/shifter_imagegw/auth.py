#!/usr/bin/env python
import munge

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

class authentication():
    """
    Authentication Class to authenticate user requests
    """

    def __init__(self,config):
        """
        Initializes authenication handle.
        config is a dictionary.  It must define 'Authentication' and it must
        be a supported type (currently munge).
        Different auth mechanisms may require additional key value pairs
        """
        if 'Authentication' not in config:
            raise KeyError('Authentication not specified')
        self.sockets=dict()
        if config['Authentication']=="munge":
            for system in config['Platforms']:
                self.sockets[system]=config['Platforms'][system]['mungeSocketPath']
            self.type='munge'
        elif config['Authentication']=="mock":
            self.type='mock'
        else:
            raise NotImplementedError('Unsupported auth type %s'%(config['Authentication']))

    def authenticate(self,authString,system=None):
        """
        authenticate a message
        authString is the message to be validated.
        system is required for munge.
        """
        if self.type=='munge':
            if authString is None:
                raise KeyError("No Auth String Provided")
            if system is None:
                raise KeyError('System must be specified for munge')
            response=munge.unmunge(authString,socket=self.sockets[system])
            if response is None:
                raise OSError('Authentication Failed')
            rv=dict()
            (user,uid)=response['UID'].replace(' ','').rstrip(')').split('(')
            (group,gid)=response['GID'].replace(' ','').rstrip(')').split('(')
            rv={'user':user,'uid':uid,'group':group,'gid':gid,'tokens':''}
            message_json=response['MESSAGE']
            try:
                rv['tokens']=json.loads(message_json)['authorized_locations']
            except:
                pass
            return rv
        elif self.type=='mock':
            rv=dict()
            if authString is None:
                raise KeyError("No Auth String Provided")
            list=authString.split(':')
            if len(list)==3:
                (status,user,group)=list
                rv={'user':user,'group':group,'tokens':''}
            elif len(list)==4:
                (status,user,gid,token)=list
                rv={'user':user,'group':group,'tokens':token}
            else:
                raise OSError('Bad AuthString')

            if status=='good':
                return rv
            else:
                raise OSError('Auth Failed st=%s'%status)
        else:
            raise OSError('Unsupported auth type')
