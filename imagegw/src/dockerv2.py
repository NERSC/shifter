import hashlib
import httplib
import ssl
import json
import os
import sys
import subprocess
import base64
import binascii
import struct
import tempfile
import socket
import socks
import urllib2

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

# Option to use a SOCKS proxy
if 'all_proxy' in os.environ:
   (socks_type,socks_host,socks_port)=os.environ['all_proxy'].split(':')
   socks_host=socks_host.replace('//','')
   socks.set_default_proxy(socks.SOCKS5, socks_host,int(socks_port))
   socket.socket = socks.socksocket  #dont add ()!!!


# TODO: Turn this into a class like dockerhub
#

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


def verifyManifestDigestAndSignature(manifest, text, hashalgo, digest):
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

def setupHttpConn(url, cacert=None):
    """
    setupHttpConn - Helper function to initialize the http connection
    returns a connection object
    """
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
        raise NotImplementedError('Unsupported protocol %s' % protocol)
    return conn

def getImageManifest(url, repo, tag, cacert=None, username=None, password=None):
    """
    getImageManifest - Get the image manifest
    returns a dictionary object of the manifest.
    """
    conn = setupHttpConn(url,cacert)
    if conn is None:
        return None

    conn.request("GET", "/v2/%s/manifests/%s" % (repo, tag))
    r1 = conn.getresponse()

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
        verifyManifestDigestAndSignature(jdata, data, digest_algo, expected_hash)
    except ValueError:
        raise e
    return jdata

def saveLayer(url, repo, layer, cachedir='./', cacert=None, username=None, password=None):
    """
    saveLayer - Save a layer and verify with the digest
    """
    conn = setupHttpConn(url, cacert)
    if conn is None:
        return None

    filename = '%s/%s.tar' % (cachedir,layer)
    if os.path.exists(filename):
        return True
    conn.request("GET", "/v2/%s/blobs/%s" % (repo, layer))
    r1 = conn.getresponse()
    #print r1.status, r1.reason
    #print r1.getheaders()
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
    if sum != value:
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
    """
    setupImageBase - Helper function to create a work area
    """
    return tempfile.mkdtemp()

def extractDockerLayers(basePath, layer, cachedir='./'):
    """
    extractDockerLayers - Recusrively Untar the layers
    """
    if layer is None:
        return
    os.umask(022)
    devnull = open(os.devnull, 'w')
    tarfile=os.path.join(cachedir,'%s.tar'%(layer['fsLayer']['blobSum']))
    command=['tar','xf', tarfile, '-C', basePath]
    if False:
        command.append('--force-local')
    ret = subprocess.call(command, stdout=devnull, stderr=devnull)
    devnull.close()
    if ret>1:
        raise OSError("Extraction of layer (%s) to %s failed %d"%(tarfile,basePath,ret))
    # ignore errors since some things like mknod are expected to fail
    extractDockerLayers(basePath, layer['child'],cachedir=cachedir)

def pullImage(options, baseUrl, repo, tag, cachedir='./', expanddir='./', cacert=None, username=None, password=None):
    """
    pullImage - Uber function to pull the manifest, layers, and extract the layers
    """
    manifest = getImageManifest(baseUrl, repo, tag, cacert, username, password)
    (eldest,youngest) = constructImageMetadata(manifest)
    layer = eldest
    while layer is not None:
        saveLayer(baseUrl, repo, layer['fsLayer']['blobSum'], cachedir, cacert, username, password)
        layer = layer['child']

    layer = eldest
    meta=youngest()
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

    extractDockerLayers(expanddir, layer, cachedir=cachedir)
    return resp

if __name__ == '__main__':
  #pullImage(None, 'https://index.docker.io', 'redis', 'latest')
  #pullImage(None, 'https://registry.services.nersc.gov', 'lcls_xfel_edison', '201509081609',cacert='local.crt')
  dir=os.getcwd()
  cdir=os.environ['TMPDIR']
  pullImage(None, 'https://registry.services.nersc.gov', 'nersc-py', 'latest',cachedir=cdir,cacert=dir+'/local.crt')
