import httplib
import re
import ssl
import json
import os
import sys
import subprocess
import base64
import tempfile
import random

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


class dockerhubHandle():
    """
    A class for fetching and unpacking dockerhub images.  DockerHub images
    are v1 like but have some special differences that require its own class.
    """

    repo = None
    tag = None
    protocol = 'https'
    server = 'index.docker.io'
    basePath = '/v1'
    cacert = None
    username = None
    password = None
    token = None
    allowAuthenticated = True

    def __init__(self, imageIdent, options = None):
        """
        Initialize an instance of the DockerHub class.
        imageIdent is an tagged repo (e.g. ubuntu:14.04)
        options is a dictionary.  Valid options include
        baseUrl to specify an URL other than dockerhub.
        cacert to specify a certificate.
        username/password to specify a login.
        """

        ## attempt to parse image identifier
        try:
            self.repo, self.tag = imageIdent.strip().split(':', 1)
        except:
            if type(imageIdent) is str:
                raise ValueError('Invalid docker image identifier: %s' % imageIdent)
            else:
                raise ValueError('Invalid type for docker image identifier')

        if options is None:
            options = {}
        if type(options) is not dict:
            raise ValueError('Invalid type for dockerhub options')

        if 'baseUrl' in options:
            baseUrlStr = options['baseUrl']
            protocol = None
            server = None
            basePath=None
            if baseUrlStr.find('http')>=0:
                protocol=baseUrlStr.split(':')[0]
                index=baseUrlStr.find(':')
                baseUrlStr=baseUrlStr[index+3:]
            if baseUrlStr.find('/')>=0:
                index=baseUrlStr.find('/')
                basePath=baseUrlStr[index:]
                server=baseUrlStr[:index]
            else:
                server=baseUrlStr

            if protocol is None or len(protocol) == 0:
                protocol = 'https'
            #print "protocol=%s server=%s basePath=%s"%(protocol,server,basePath)

            if server is None or len(server) == 0:
                raise ValueError('unable to parse baseUrl, no server specified, should be like https://server.location/optionalBasePath')

            if basePath is None:
                basePath = '/v1'

            self.protocol = protocol
            self.server = server
            self.basePath = basePath

        if self.protocol == 'http':
            self.allowAuthenticated = False

        if 'cacert' in options:
            if not os.path.exists(options['cacert']):
                raise ValueError('specified cacert file does not exist: %s' % options['cacert'])

            self.cacert = options['cacert']

        if 'username' in options:
            self.username = options['username']
        if 'password' in options:
            self.password = options['password']

        if (self.password is not None and self.username is None) or (self.username is not None and self.password is None):
            raise ValueError('if either username or password is specified, both must be')

        if self.allowAuthenticated == False and self.username is not None:
            raise ValueError('authentication not allowed with the current settings (make sure you are using https)')

        self.setupDockerHub()

        if self.token is None or len(self.token) == 0:
            raise ValueError('Failed to get authorization for repo %s' % self.repo)

        ## if we didn't get any endpoints, use the already-defined server
        if self.endpoints is None or type(self.endpoints) != list or len(self.endpoints) == 0:
            self.endpoints = [self.server]

        ## see if we can find this tag
        self.lookupTag()
        if self.taghash is None or len(self.taghash) == 0:
            raise ValueError('failed to lookup requested image: %s:%s' % (self.repo, self.tag))

        ## get image layers
        self.layers = self.getImageAncestry()

        ## get image manifest by looking up layer metadata
        self.layerMetadata = {}
        for layer in self.layers:
            self.layerMetadata[layer] = self.getLayerMetadata(layer)
        self.constructImageMetadata()


    def pullLayers(self,directory='./'):
        """
        Pull the layers down and save them to local storage.
        """

        layer=self.taghash
        while layer is not None:
            fname=os.path.join(directory,"%s.tar"%layer)
            if not os.path.exists(fname):
                self.getLayer(layer,fname)
            if 'parent' in self.layerMetadata[layer]:
                child=layer
                layer=self.layerMetadata[layer]['parent']
                self.layerMetadata['child']=self.layerMetadata[child]
            else:
                layer=None

    def setupHttpConn(self, url, cacert=None):
        (protocol, url) = url.split('://', 1)
        location = None
        conn = None
        if (url.find('/') >= 0):
            (server, location) = url.split('/', 1)
        else:
            server = url
        if protocol == 'http':
            conn = httplib.HTTPConnection(server)
        elif protocol == 'https':
            sslContext = ssl.create_default_context()

            if cacert is not None:
                sslContext = ssl.create_default_context(cafile=cacert)
            conn = httplib.HTTPSConnection(server, context=sslContext)
        else:
            print "Error, unknown protocol %s" % protocol
            return None
        return conn

    def setupDockerHub(self):
        """
        Helper function to setup the DockerHub connection
        """
        baseUrl = '%s://%s' % (self.protocol, self.server)
        conn = self.setupHttpConn(baseUrl, self.cacert)
        headers = {'X-Docker-Token': 'True'}
        if self.username is not None and self.password is not None:
            headers['Authorization'] = 'Basic %s' % base64.b64encode('%s:%s' % (self.username, self.password))
        conn.request("GET", "/v1/repositories/%s/images" % (self.repo), None, headers)
        r1 = conn.getresponse()

        if r1.status != 200:
            raise ValueError('Bad response from registry: %d', r1.status)

        endpoint_string = r1.getheader('x-docker-endpoints')
        self.token = r1.getheader('x-docker-token')
        self.endpoints = endpoint_string.strip().split(' ')
        matchObj = re.search(r'repository="(.*?)"', self.token)
        if matchObj is not None:
            self.repo = matchObj.groups()[0]
        else:
            raise ValueError('Failed to parse repository from returned token: %s' % self.token)

    def getEldest(self):
        return self.eldest

    def getYoungest(self):
        return self.youngest

    def getTag(self):
        if self.taghash is None:
            self.lookupTag()
        return self.taghash

    def lookupTag(self):
        """
        Retrieve the tag of the image.
        """
        conn = self.setupHttpConn('https://%s' % self.endpoints[0], self.cacert)
        conn.request('GET', '/v1/repositories/%s/tags/%s' % (self.repo, self.tag), None, {'Authorization': 'Token %s' % self.token})
        r1 = conn.getresponse()

        if r1.status != 200:
            raise ValueError('Bad response from registry')
        tag = r1.read().replace('"', '')
        self.taghash = tag
        return tag

    def getImageAncestry(self):
        conn = self.setupHttpConn('https://%s' % self.endpoints[0], self.cacert)
        conn.request('GET', '/v1/images/%s/ancestry' % self.taghash, None, {'Authorization': 'Token %s' % self.token})
        r1 = conn.getresponse()
        if r1.status != 200:
            raise ValueError('Bad response from registry')
        if r1.getheader('content-type') == 'application/json':
            data = r1.read()
            return json.loads(data)
        return None

    def getLayer(self, layerHash, fname):
        """
        Pull down a specific layer.
        """
        endpointIdx = random.randrange(0, len(self.endpoints)) % len(self.endpoints)
        conn = self.setupHttpConn('%s://%s' % (self.protocol, self.endpoints[endpointIdx]), self.cacert)
        conn.request('GET', '/v1/images/%s/layer' % layerHash, None, {'Authorization': 'Token %s' % self.token})
        r1 = conn.getresponse()

        while r1 is not None and r1.status == 302:
            loc = r1.getheader('location')
            protocol,fullloc = loc.split('://', 1)
            server,path = fullloc.split('/', 1)
            conn = self.setupHttpConn('%s://%s' % (protocol, server), self.cacert)
            conn.request('GET', '/' + path)
            r1 = conn.getresponse()
        if r1.status != 200:
            raise ValueError('Bad response from registry')

        contentLen = int(r1.getheader('content-length'))
        with open(fname,'w') as fp:
            fp.write(r1.read())
            fp.close()

    def getLayerMetadata(self, layerHash):
        """
        Retrieve the metadata for a specific layer and unpack it.
        """
        endpointIdx = random.randrange(0, len(self.endpoints)) % len(self.endpoints)
        conn = self.setupHttpConn('%s://%s' % (self.protocol, self.endpoints[endpointIdx]), self.cacert)
        conn.request('GET', '/v1/images/%s/json' % layerHash, None, {'Authorization': 'Token %s' % self.token})
        r1 = conn.getresponse()

        if r1.status != 200:
            raise ValueError('Bad response from registry')
        if r1.getheader('content-type') != 'application/json':
            raise ValueError('Bad reponse from registry: content not json')
        return json.loads(r1.read())

    def extractDockerLayers(self,basePath, layer,cachedir="./"):
        """
        Extract Image Layers.
        basePath is where the layers will be extracted to
        layer is a linked list of layers
        """
        if layer is None:
            return
        os.umask(022)
        devnull = open(os.devnull, 'w')
        tarfile=os.path.join(cachedir,'%s.tar'%(layer['id']))
        com=['tar','xf', tarfile, '-C', basePath]
        if False:
            command.append('--force-local')
        ret = subprocess.call(com, stdout=devnull, stderr=devnull)
        devnull.close()
        if ret>1:
            raise OSError("Extraction of layer (%s) to %s failed %d"%(tarfile,basePath,ret))
        # ignore errors since some things like mknod are expected to fail
        self.extractDockerLayers(basePath, layer['child'],cachedir=cachedir)

    def constructImageMetadata(self):
        """
        construct additional metadata.
        """

        layer=self.taghash
        self.youngest=self.layerMetadata[layer]
        prev=None
        while layer is not None:
            self.layerMetadata[layer]['child']=prev
            if 'parent' in self.layerMetadata[layer]:
                prev=self.layerMetadata[layer]
                layer=self.layerMetadata[layer]['parent']
            else:
                self.eldest=self.layerMetadata[layer]
                layer=None
        layer=self.eldest
        while layer!=None:
            layer=layer['child']


def pullImage(options, baseUrl, repo, tag, cachedir='./', expanddir='./', cacert=None, username=None, password=None):
    """
    Macro function that pulls and extracts the image.
    This may go away once we create a base class.
    """
    if options is None:
        options=dict()
    if username is not None:
        options['username']=username
    if password is not None:
        options['password']=password
    if cacert is not None:
        options['cacert']=cacert
    if baseUrl is not None:
        options['baseUrl']=baseUrl
    imageident='%s:%s'%(repo,tag)
    a=dockerhubHandle(imageident,options)
    a.pullLayers(cachedir)
    meta=a.getYoungest()
    resp={'id':meta['id']}
    expandedpath=os.path.join(expanddir,str(meta['id']))
    resp['expandedpath']=expandedpath
    if 'config' in meta:
        c=meta['config']
        if 'Env' in c:
            resp['env']=c['Env']
        if 'Entrypoint' in c:
            resp['entrypoint']=c['Entrypoint']
    if not os.path.exists(expandedpath):
        os.mkdir(expandedpath)
    a.extractDockerLayers(expandedpath,a.getEldest(),cachedir=cachedir)
    return resp

if __name__ == '__main__':
    pullImage(None, 'https://index.docker.io', 'ubuntu','latest', cachedir='/tmp/cache/', expanddir='/tmp/ubuntu/')
    pullImage(None, 'index.docker.io', 'ubuntu','latest', cachedir='/tmp/cache/', expanddir='/tmp/ubuntu/')
    pullImage(None, None, 'ubuntu','latest', cachedir='/tmp/cache/', expanddir='/tmp/ubuntu/')
    #pullImage(None, 'https://registry.services.nersc.gov', 'lcls_xfel_edison', '201509081609', cacert='local.crt')
