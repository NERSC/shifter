#!/usr/bin/env python


import munge

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
            uid=response['UID']
            gid=response['GID']
            return (uid,gid)
        elif self.type=='mock':
            if authString is None:
                raise KeyError("No Auth String Provided")
            elif authString.startswith('good:'):
                (type,uid,gid)=authString.split(':')
                return (uid,gid)
            else:
                raise OSError('Auth Failed')
        else:
            raise OSError('Unsupported auth type')
