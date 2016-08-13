import hashlib
import httplib
import ssl
import json
import os
import sys
import re
import subprocess
import base64
import binascii
import struct
import tempfile
import socket
import urllib2
import shifter_imagegw
import stat
import shutil
from time import time

## Shifter, Copyright (c) 2015, The Regents of the University of California,
## through Lawrence Berkeley National Laboratory (subject to receipt of any
## required approvals from the U.S. Dept. of Energy).  All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##  1. Redistributions of source code must retain the above copyright notice,
##     this list of conditions and the following disclaimer.
##  2. Redistributions in binary form must reproduce the above copyright notice,
##     this list of conditions and the following disclaimer in the documentation
##     and/or other materials provided with the distribution.
##  3. Neither the name of the University of California, Lawrence Berkeley
##     National Laboratory, U.S. Dept. of Energy nor the names of its
##     contributors may be used to endorse or promote products derived from this
##     software without specific prior written permission.`
##
## See LICENSE for full text.

# Option to use a SOCKS proxy
if 'all_proxy' in os.environ:
    import socks
    (socks_type,socks_host,socks_port)=os.environ['all_proxy'].split(':')
    socks_host=socks_host.replace('//','')
    socks.set_default_proxy(socks.SOCKS5, socks_host,int(socks_port))
    socket.socket = socks.socksocket  #dont add ()!!!


def joseDecodeBase64(input):
    """
    Helper function to Decode base64
    """
    bytes = len(input) % 4
    if bytes == 0 or bytes == 2:
        return base64.b64decode(input + '==')
    if bytes == 3:
        return base64.b64decode(input + '===')
    return base64.b64decode(input)

class dockerv2Handle():
    """
    A class for fetching and unpacking docker registry (and dockerhub) images.
    This class is a client of the Docker Registry V2 protocol.
    """

    repo = None
    tag = None
    protocol = 'https'
    server = 'registry-1.docker.io'
    basePath = '/v2'
    cacert = None
    username = None
    password = None
    token = None
    allowAuthenticated = True
    checkLayerChecksums = True

    # excluding this blobSum because it translates to an empty tar file
    # and python 2.6 throws an exception when an open is attempted
    excludeBlobSums = ['sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4']

    def __init__(self, imageIdent, options = None, updater=None):
        """
        Initialize an instance of the dockerv2 class.
        imageIdent is a tagged repo (e.g., ubuntu:14.04)
        options is a dictionary.  Valid options include:
            baseUrl to specify a URL other than dockerhub
            cacert to specify an approved signing authority
            username/password to specify a login
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
            raise ValueError('Invalid type for dockerv2 options')
        self.updater=updater

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

            if server is None or len(server) == 0:
                raise ValueError('unable to parse baseUrl, no server specified, should be like https://server.location/optionalBasePath')

            if basePath is None:
                basePath = '/v2'

            self.protocol = protocol
            self.server = server
            self.basePath = basePath

        if self.protocol == 'http':
            self.allowAuthenticated = False

        self.url = '%s://%s' % (self.protocol, self.server)

        if self.repo.find('/') == -1 and self.server.endswith('docker.io'):
            self.repo = 'library/%s' % self.repo

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

        self.authMethod = 'token'
        if 'authMethod' in options:
            self.authMethod = options['authMethod']
        self.eldest=None
        self.youngest=None

    def get_eldest(self):
        return self.eldest

    def log(self,state,message=''):
        if self.updater is not None:
            self.updater.update_status(state,message)

    def excludeLayer(self, blobsum):
        ## TODO: add better verfication of the blobsum, potentially give other
        ## routes to mask out a layer with this function
        if blobsum not in self.excludeBlobSums:
            self.excludeBlobSums.append(blobsum)

    def setupHttpConn(self, url, cacert=None):
        (protocol, url) = url.split('://', 1)
        location = None
        conn = None
        port = 443
        if (url.find('/') >= 0):
            (server, location) = url.split('/', 1)
        else:
            server = url
        if ':' in server:
            (server, port) = server.split(':')
        if protocol == 'http':
            conn = httplib.HTTPConnection(server)
        elif protocol == 'https':
            try:
                sslContext = ssl.create_default_context()
                if cacert is not None:
                    sslContext = ssl.create_default_context(cafile=cacert)
                conn = httplib.HTTPSConnection(server, context=sslContext)
            except AttributeError:
                conn = httplib.HTTPSConnection(server, port, None, cacert)
        else:
            print "Error, unknown protocol %s" % protocol
            return None
        return conn

    def doTokenAuth(self, authLocStr):
        """
        Perform token authorization as specified by Docker registry v2 specification.
        authLocStr is a string returned in the "WWW-Authenticate" response header
        It contains:
            <mode> realm=<authUrl>,service=<service>,scope=<scope>
            The mode will typically be "bearer", service and scope are the repo and
            capabilities being requested respectively.  For shifter, the scope will
            only be pull.

        """
        mode = None
        authDataStr = None

        (mode, authDataStr) = authLocStr.split(' ', 2)

        authData = {}
        for item in authDataStr.split(','):
            (k,v) = item.split('=', 2)
            authData[k] = v.replace('"', '')

        authConn = self.setupHttpConn(authData['realm'], self.cacert)
        if authConn is None:
            raise ValueError('Bad response from registry, failed to get auth connection')

        headers = {}
        if self.username is not None and self.password is not None:
            headers['Authorization'] = 'Basic %s' % base64.b64encode('%s:%s' % (self.username, self.password))

        matchObj = re.match(r'(https?)://(.*?)(/.*)', authData['realm'])
        path = '%s?service=%s&scope=%s' % (matchObj.groups()[2], authData['service'], authData['scope'])
        authConn.request("GET", path, None, headers)
        r2 = authConn.getresponse()

        if r2.status != 200:
            raise ValueError('Bad response getting token: %d', r2.status)
        if r2.getheader('content-type') != 'application/json':
            raise ValueError('Invalid response getting token, not json')

        authResp = json.loads(r2.read())
        self.token = authResp['token']

    def verifyManifestDigestAndSignature(self, manifest, text, hashalgo, digest):
        """
        verifyManifestDigestAndSignature - Verify the manifest
        """
        formatLen = None
        formatTail = None

        if 'signatures' in manifest:
            for idx,sig in enumerate(manifest['signatures']):
                protectedStr =  joseDecodeBase64(sig['protected'])
                protected = json.loads(protectedStr)
                lformatTail = joseDecodeBase64(protected['formatTail'])
                if formatTail is None:
                    formatTail = lformatTail
                elif formatTail != lformatTail:
                    raise ValueError('formatTail did not match between signature blocks')
                if formatLen is None:
                    formatLen = protected['formatLength']
                elif formatLen != protected['formatLength']:
                    raise ValueError('formatLen did not match between signature blocks')
        message = text[0:formatLen] + formatTail
        if hashlib.sha256(message).hexdigest() != digest:
            raise ValueError("Failed to match manifest digest to downloaded content")

        return True

    def getImageManifest(self, retrying=False):
        """
        getImageManifest - Get the image manifest
        returns a dictionary object of the manifest.
        """
        conn = self.setupHttpConn(self.url,self.cacert)
        if conn is None:
            return None

        headers = {}
        if self.authMethod == 'token' and self.token is not None:
            headers = {'Authorization': 'Bearer %s' % self.token}
        elif self.authMethod == 'basic' and self.username is not None:
            headers['Authorization'] = 'Basic %s' % base64.b64encode('%s:%s' % (self.username, self.password))

        conn.request("GET", "/v2/%s/manifests/%s" % (self.repo, self.tag), None, headers)
        r1 = conn.getresponse()

        if r1.status == 401 and not retrying:
            if self.authMethod == 'token':
                self.doTokenAuth(r1.getheader('WWW-Authenticate'))
            return self.getImageManifest(retrying=True)

        if r1.status != 200:
            raise ValueError("Bad response from registry status=%d"%(r1.status))
        expected_hash = r1.getheader('docker-content-digest')
        content_len = r1.getheader('content-length')
        if expected_hash is None or len(expected_hash) == 0:
            raise ValueError("No docker-content-digest header found")
        (digest_algo, expected_hash) = expected_hash.split(':', 1)
        data = r1.read()
        jdata = json.loads(data)
        try:
            self.verifyManifestDigestAndSignature(jdata, data, digest_algo, expected_hash)
        except ValueError:
            raise e
        return jdata

    def examine_manifest(self,manifest):
        self.log("PULLING",'Constructing manifest')
        (eldest,youngest) = self.constructImageMetadata(manifest)

        self.eldest=eldest
        self.youngest=youngest
        meta=youngest

        resp={'id':meta['id']}
        if 'config' in meta:
            c=meta['config']
            if 'Env' in c:
                resp['env']=c['Env']
            if 'Entrypoint' in c:
                resp['entrypoint']=c['Entrypoint']
        return resp


    def pull_layers(self,manifest,cachedir):
        if self.eldest is None:
            resp = self.examine_manifest(manifest)
        layer = self.eldest
        while layer is not None:
            if layer['fsLayer']['blobSum'] in self.excludeBlobSums:
                layer = layer['child']
                continue

            self.log("PULLING","Pulling layer %s"%layer['fsLayer']['blobSum'])
            self.saveLayer(layer['fsLayer']['blobSum'], cachedir)
            layer = layer['child']
        return True

    def saveLayer(self, layer, cachedir='./'):
        """
        saveLayer - Save a layer and verify with the digest
        """
        path = "/v2/%s/blobs/%s" % (self.repo, layer)
        url = self.url
        output_fname = None
        while True:
            conn = self.setupHttpConn(url, self.cacert)
            if conn is None:
                return None

            headers = {}
            if self.authMethod == 'token' and self.token is not None:
                headers = {'Authorization': 'Bearer %s' % self.token}
            elif self.authMethod == 'basic' and self.username is not None:
                headers['Authorization'] = 'Basic %s' % base64.b64encode('%s:%s' % (self.username, self.password))

            filename = '%s/%s.tar' % (cachedir,layer)

            if os.path.exists(filename):
                try:
                    return self.checkLayerChecksum(layer, filename)
                except ValueError:
                    # there was a checksum mismatch, nuke the file
                    os.unlink(filename)

            conn.request("GET", path, None, headers)
            r1 = conn.getresponse()
            location = r1.getheader('location')
            if r1.status == 200:
                break
            elif r1.status == 401:
                if self.authMethod == 'token':
                    self.doTokenAuth(r1.getheader('WWW-Authenticate'))
                    next
            elif location != None:
                url = location
                matchObj = re.match(r'(https?)://(.*?)(/.*)', location)
                url = '%s://%s' % (matchObj.groups()[0], matchObj.groups()[1])
                path = matchObj.groups()[2]
            else:
                print 'got status: %d' % r1.status
                return False
        maxlen = int(r1.getheader('content-length'))
        nread = 0
        (output_fd, output_fname) = tempfile.mkstemp('.partial', layer, cachedir)
        output_fp = os.fdopen(output_fd, 'w')

        try:
            readsz = 4 * 1024 * 1024 # read 4MB chunks
            while nread < maxlen:
                buff = r1.read(readsz)
                if buff is None:
                    break

                output_fp.write(buff)
                nread += len(buff)
            output_fp.close()
            self.checkLayerChecksum(layer, output_fname)
        except:
            os.unlink(output_fname)
            raise

        os.rename(output_fname, filename)
        return True

    def checkLayerChecksum(self, layer, filename):
        if self.checkLayerChecksums is False:
            return True

        (hashType,value) = layer.split(':', 1)
        execName = '%s%s' % (hashType, 'sum')
        process = subprocess.Popen([execName, filename], stdout=subprocess.PIPE)

        (stdoutData, stderrData) = process.communicate()
        (sum,other) = stdoutData.split(' ', 1)
        if sum != value:
            raise ValueError("checksum mismatch, failure")
        return True

    def constructImageMetadata(self, manifest):
        if manifest is None:
            raise ValueError('Invalid manifest')
        if 'schemaVersion' not in manifest or manifest['schemaVersion'] != 1:
            raise ValueError('Incompatible manifest schema')
        if 'fsLayers' not in manifest or 'history' not in manifest or 'signatures' not in manifest:
            raise ValueError('Manifest in incorrect format')
        if len(manifest['fsLayers']) != len(manifest['history']):
            raise ValueError('Manifest layer size mismatch')
        layers = {}
        noParent = None
        for idx,layer in enumerate(manifest['history']):
            if 'v1Compatibility' not in layer:
                raise ValueError('Unknown layer format')
            layerData = json.loads(layer['v1Compatibility'])
            layerData['fsLayer'] = manifest['fsLayers'][idx]
            if 'parent' not in layerData:
                if noParent is not None:
                    raise ValueError('Found more than one layer with no parent, cannot proceed')
                noParent = layerData
            else:
                if layerData['parent'] in layers:
                    if layers[layerData['parent']]['id'] == layerData['id']:
                        # skip already-existing image
                        continue
                    raise ValueError('Multiple inheritance from a layer, unsure how to proceed')
                layers[layerData['parent']] = layerData
            if 'id' not in layerData:
                raise ValueError('Malformed layer, missing id')

        if noParent is None:
            raise ValueError("Unable to find single layer wihtout parent, cannot identify terminal layer")

        # traverse graph and construct linked-list of layers
        curr = noParent
        count = 1
        while (curr is not None):
            if curr['id'] in layers:
                curr['child'] = layers[curr['id']]
                del layers[curr['id']]
                curr = curr['child']
                count += 1
            else:
                curr['child'] = None
                break

        return (noParent,curr,)

    def setupImageBase(self, options):
        """
        setupImageBase - Helper function to create a work area
        """
        return tempfile.mkdtemp()

    def extractDockerLayers(self, basePath, baseLayer, cachedir='./'):
        import tarfile
        def filterLayer(layerMembers, toRemove):
            prefixToRemove = '%s%s' % (toRemove, '/' if not toRemove.endswith('/') else '')
            return [ x for x in layerMembers if not x.name == toRemove and not x.name.startswith(prefixToRemove) ]

        layerPaths = []
        layer = baseLayer
        while layer is not None:
            if layer['fsLayer']['blobSum'] in self.excludeBlobSums:
                layer = layer['child']
                continue

            tfname = os.path.join(cachedir,'%s.tar'%(layer['fsLayer']['blobSum']))
            tfp = tarfile.open(tfname, 'r:gz')

            ## get directory of tar contents
            layerMembers = tfp.getmembers()

            ## remove all illegal files
            layerMembers = filterLayer(layerMembers, 'dev/')
            layerMembers = filterLayer(layerMembers, '/')
            layerMembers = [ x for x in layerMembers if not x.name.find('..') >= 0 ]

            ## find all whiteouts
            whiteouts = [ x for x in layerMembers if (x.name.find('/.wh.') >= 0
                    or x.name.startswith('.wh.')) ]

            ## remove the whiteout tags from this layer
            for wh in whiteouts:
                layerMembers.remove(wh)

            ## remove the whiteout targets from all ancestral layers
            for idx,ancsLayer in enumerate(layerPaths):
                for wh in whiteouts:
                    path = wh.name.replace('/.wh.', '/')
                    if path.startswith('.wh.'):
                        path = path[4:]
                    ancsLayerIter = (x for x in ancsLayer if x.name == path)
                    ancsMember = next(ancsLayerIter, None)
                    if ancsMember:
                        ancsLayer = filterLayer(ancsLayer, path)
                layerPaths[idx] = ancsLayer

            ## remove identical paths (not dirs) from all ancestral layers
            notdirs = [ x.name for x in layerMembers if not x.isdir() ]
            for idx,ancsLayer in enumerate(layerPaths):
                ancsLayer = [ x for x in ancsLayer if not x.name in notdirs ]
                layerPaths[idx] = ancsLayer

            ## push this layer into the collection
            layerPaths.append(layerMembers)
            tfp.close()

            layer = layer['child']


        ## extract the selected files
        layerIdx = 0
        layer = baseLayer
        while layer is not None:
            if layer['fsLayer']['blobSum'] in self.excludeBlobSums:
                layer = layer['child']
                continue

            tfname = os.path.join(cachedir,'%s.tar' % (layer['fsLayer']['blobSum']))
            tfp = tarfile.open(tfname, 'r:gz')
            members = layerPaths[layerIdx]
            tfp.extractall(path=basePath,members=members)

            layerIdx += 1
            layer = layer['child']

        ## fix permissions on the extracted files
        subprocess.call(['chmod', '-R', 'a+rX,u+w', basePath])

# Deprecated: Just use the object above
def pullImage(options, baseUrl, repo, tag, cachedir='./', expanddir='./', cacert=None, username=None, password=None):
    """
    pullImage - Uber function to pull the manifest, layers, and extract the layers
    """
    token = None
    if options is None:
        options = {}
    if username is not None:
        options['username'] = username
    if password is not None:
        options['password'] = password
    if cacert is not None:
        options['cacert'] = cacert
    if baseUrl is not None:
        options['baseUrl'] = baseUrl
    imageident = '%s:%s' % (repo, tag)
    a = dockerv2Handle(imageident, options)

    print "about to get manifest"
    manifest = a.getImageManifest()
    (eldest,youngest) = a.constructImageMetadata(manifest)
    layer = eldest
    print "about to get layers"
    while layer is not None:
        print "about to pull layer: ", layer['fsLayer']['blobSum']
        a.saveLayer(layer['fsLayer']['blobSum'], cachedir)
        layer = layer['child']

    layer = eldest
    meta=youngest
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

    a.extractDockerLayers(expandedpath, layer, cachedir=cachedir)
    return resp

if __name__ == '__main__':
  dir=os.getcwd()
  cdir=os.environ['TMPDIR']
  #pullImage(None, 'https://registry.services.nersc.gov', 'ana', 'cctbx',cachedir=cdir,expanddir=cdir,cacert=dir+'/local.crt')
  #pullImage(None, 'https://registry-1.docker.io', 'ubuntu', 'latest', cachedir=cdir, expanddir=cdir)
  #pullImage(None, 'https://registry-1.docker.io', 'tensorflow/tensorflow', 'latest', cachedir=cdir, expanddir=cdir)
  pullImage(None, 'https://registry-1.docker.io', 'dmjacobsen/ltp-test', 'latest', cachedir=cdir, expanddir=cdir)
