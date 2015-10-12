import httplib
import re
import ssl
import json
import os
import sys
import subprocess
import base64
import binascii
import struct
import tempfile
import random

class dockerhubHandle:
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
            basePath = ''
            matchObj = re.match(r'((https?)://)?(.*?)(/.*)', baseUrlStr)
            if matchObj is None:
                if baseUrlStr.find('/') >= 0:
                    raise ValueError('unable to parse baseUrl, should be like https://server.location/optionalBasePath')
                protocol = 'https'
                server = baseUrlStr
                basePath = ''
            else:
                protocol = matchObj.groups()[1]
                server = matchObj.groups()[2]
                basePath = matchObj.groups()[3]
            if protocol is None or len(protocol) == 0:
                protocol = 'https'

            if server is None or len(server) == 0:
                raise ValueError('unable to parse baseUrl, no server specified, should be like https://server.location/optionalBasePath')

            if basePath is None:
                basePath = ''
            
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

    def lookupTag(self):
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
        fp = open(fname, 'w')
        fp.write(r1.read())
        fp.close()

    def getLayerMetadata(self, layerHash):
        endpointIdx = random.randrange(0, len(self.endpoints)) % len(self.endpoints)
        conn = self.setupHttpConn('%s://%s' % (self.protocol, self.endpoints[endpointIdx]), self.cacert)
        conn.request('GET', '/v1/images/%s/json' % layerHash, None, {'Authorization': 'Token %s' % self.token})
        r1 = conn.getresponse()

        if r1.status != 200:
            raise ValueError('Bad response from registry')
        if r1.getheader('content-type') != 'application/json':
            raise ValueError('Bad reponse from registry: content not json')
        return json.loads(r1.read())


def getImageManifest(url, repo, tag, cacert=None, username=None, password=None):
    conn = setupHttpConn(url, cacert)
    if conn is None:
        return None

    conn.request("GET", "/v2/%s/manifests/%s" % (repo, tag))
    r1 = conn.getresponse()

    if r1.status != 200:
        raise ValueError("Bad response from registry")
    expected_hash = r1.getheader('docker-content-digest')
    content_len = r1.getheader('content-length')
    if expected_hash is None or len(expected_hash) == 0:
        raise ValueError("No docker-content-digest header found")
    (digest_algo, expected_hash) = expected_hash.split(':', 1)
    data = r1.read()
    jdata = json.loads(data)
    try:
        verifyManifestDigestAndSignature(jdata, data, digest_algo, expected_hash)
    except ValueError:
        raise e
    return jdata

def saveLayer(url, repo, layer, cacert=None, username=None, password=None):
    conn = setupHttpConn(url, cacert)
    if conn is None:
        return None

    filename = '%s.tar' % layer
    if os.path.exists(filename):
        return True
    conn.request("GET", "/v2/%s/blobs/%s" % (repo, layer))
    r1 = conn.getresponse()
    print r1.status, r1.reason
    print r1.getheaders()
    maxlen = int(r1.getheader('content-length'))
    nread = 0
    output = open(filename, "w")
    readsz = 4 * 1024 * 1024 # read 4MB chunks
    while nread < maxlen:
        buff = r1.read(readsz)
        if buff is None:
            break
        if type(buff) != str:
            print buff
            
        output.write(buff)
        nread += len(buff)
    output.close()

    (hashType,value) = layer.split(':', 1)
    execName = '%s%s' % (hashType, 'sum')
    process = subprocess.Popen([execName, filename], stdout=subprocess.PIPE)
    (stdoutData, stderrData) = process.communicate()
    (sum,other) = stdoutData.split(' ', 1)
    if sum == value:
        print "got match: %s == %s" % (sum, value)
    else:
        raise ValueError("checksum mismatch, failure")
    return True

def constructImageMetadata(manifest):
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

def setupImageBase(options):
    return tempfile.mkdtemp()

def extractDockerLayers(basePath, layer):
    if layer is None:
        return
    os.umask(022)
    devnull = open(os.devnull, 'w')
    ret = subprocess.call(['tar','xf', '%s.tar' % layer['fsLayer']['blobSum'], '-C', basePath, '--force-local'], stdout=devnull, stderr=devnull)
    devnull.close()
    # ignore errors since some things like mknod are expected to fail
    extractDockerLayers(basePath, layer['child'])

def setupDockerHub(options, baseUrl, repo, cacert=None, username=None, password=None):
    conn = setupHttpConn(baseUrl, cacert)
    headers = {'X-Docker-Token': 'True'}
    if username is not None and password is not None:
        headers['Authorization'] = 'Basic %s' % base64.b64encode('%s:%s' % (username, password))
    conn.request("GET", "/v1/repositories/%s/images" % (repo), None, headers)
    r1 = conn.getresponse()

    if r1.status != 200:
        raise ValueError('Bad response from registry')

    print r1.getheaders()
    endpoint_string = r1.getheader('x-docker-endpoints')
    token = r1.getheader('x-docker-token')
    ret = {
        'endpoints': endpoint_string.strip().split(' '),
        'token': token,
        'repository': re.search(r'repository="(.*?)"', token).groups()[0],
    }
    return ret

def lookupTag(options, dhData, repo, tag, cacert=None, username=None, password=None):
    conn = setupHttpConn('https://%s' % dhData['endpoints'][0], cacert)
    conn.request('GET', '/v1/repositories/%s/tags/%s' % (dhData['repository'], tag), None, {'Authorization': 'Token %s' % dhData['token']})
    r1 = conn.getresponse()

    if r1.status != 200:
        raise ValueError('Bad response from registry')
    tag = r1.read().replace('"', '')
    return tag

def getImageAncestry(options, dhData, tagHash, cacert=None):
    conn = setupHttpConn('https://%s' % dhData['endpoints'][0], cacert)
    conn.request('GET', '/v1/images/%s/ancestry' % tagHash, None, {'Authorization': 'Token %s' % dhData['token']})
    r1 = conn.getresponse()
    if r1.status != 200:
        raise ValueError('Bad response from registry')
    if r1.getheader('content-type') == 'application/json':
        data = r1.read()
        return json.loads(data)
    return None

def getLayer(cnt, options, dhData, layerHash, cacert=None):
    endpointIdx = cnt % len(dhData['endpoints'])
    conn = setupHttpConn('https://%s' % dhData['endpoints'][endpointIdx], cacert)
    conn.request('GET', '/v1/images/%s/layer' % layerHash, None, {'Authorization': 'Token %s' % dhData['token']})
    r1 = conn.getresponse()

    while r1 is not None and r1.status == 302:
        loc = r1.getheader('location')
        protocol,fullloc = loc.split('://', 1)
        server,path = fullloc.split('/', 1)
        conn = setupHttpConn('%s://%s' % (protocol, server), cacert)
        conn.request('GET', '/' + path)
        r1 = conn.getresponse()
    if r1.status != 200:
        raise ValueError('Bad response from registry')

    contentLen = int(r1.getheader('content-length'))
    print "writing %d bytes to %s.tar.gz" % (contentLen, layerHash)
    fp = open('%s.tar.gz' % layerHash, 'w')
    fp.write(r1.read())
    fp.close()

def getLayerMetadata(cnt, options, dhData, layerHash, cacert=None):
    endpointIdx = cnt % len(dhData['endpoints'])
    conn = setupHttpConn('https://%s' % dhData['endpoints'][endpointIdx], cacert)
    conn.request('GET', '/v1/images/%s/json' % layerHash, None, {'Authorization': 'Token %s' % dhData['token']})
    r1 = conn.getresponse()

    if r1.status != 200:
        raise ValueError('Bad response from registry')
    if r1.getheader('content-type') != 'application/json':
        raise ValueError('Bad reponse from registry: content not json')
    fp = open('%s.json' % layerHash, 'w')
    fp.write(r1.read())
    fp.close()


def pullImage(options, baseUrl, repo, tag, cacert=None, username=None, password=None):
    print username
    print password
    dockerhubData = setupDockerHub(options, baseUrl, repo, cacert, username, password)
    print dockerhubData
    tagHash = lookupTag(options, dockerhubData, repo, tag, cacert, username, password)
    print tagHash
    layerIds = getImageAncestry(options, dockerhubData, tagHash, cacert)
    for cnt,layer in enumerate(layerIds):
        getLayer(cnt, options, dockerhubData, layer, cacert)

a = dockerhubHandle('ubuntu:latest')
print a
#pullImage(None, 'https://registry.services.nersc.gov', 'lcls_xfel_edison', '201509081609', cacert='local.crt')
#pullImage(None, 'https://index.docker.io', 'dmjacobsen/testrepo1', 'latest', username='dmjacobsen', password='tEul3Zib9wIgh0eK1Car')#, cacert='local.crt')
