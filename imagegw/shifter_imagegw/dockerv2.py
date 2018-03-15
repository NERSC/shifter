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
This modules implements the logic to interact with a dockver v2 registry.  This
includes pulling down the manifest, pulling layers, and unpacking the layers.
"""

import hashlib
import urlparse
import httplib
import ssl
import json
import os
import stat
import re
from subprocess import Popen, PIPE
import base64
import tempfile
import socket
import tarfile

_EMPTY_TAR_SHA256 = \
    'sha256:a3ed95caeb02ffe68cdd9fd84406680ae93d633cb16422d00e8a7c22955b46d4'

# Option to use a SOCKS proxy
if 'all_proxy' in os.environ:
    import socks
    (SOCKS_TYPE, SOCKS_HOST, SOCKS_PORT) = os.environ['all_proxy'].split(':')
    SOCKS_HOST = SOCKS_HOST.replace('//', '')
    socks.set_default_proxy(socks.SOCKS5, SOCKS_HOST, int(SOCKS_PORT))
    socket.socket = socks.socksocket  # dont add ()!!!


def need_proxy(hostname):
    """
    Helper function to determine if a proxy should be used to connect to a host
    """
    if 'no_proxy' in os.environ and os.environ['no_proxy']:
        domains = os.environ['no_proxy'].split(',')
        ismatch = True in map(lambda x: hostname.endswith(x), domains)
        return not ismatch
    else:
        return True


def _jose_decode_base64(input_string):
    """
    Helper function to Decode base64
    """
    nbytes = len(input_string) % 4
    if nbytes == 0 or nbytes == 2:
        return base64.b64decode(input_string + '==')
    if nbytes == 3:
        return base64.b64decode(input_string + '===')
    return base64.b64decode(input_string)


def _verify_manifest_signature(manifest, text, digest):
    """
    Verify the manifest digest and signature
    """
    format_length = None
    format_tail = None

    if 'signatures' in manifest:
        for sig in manifest['signatures']:
            protected_json = _jose_decode_base64(sig['protected'])
            protected = json.loads(protected_json)
            curr_tail = _jose_decode_base64(protected['formatTail'])
            if format_tail is None:
                format_tail = curr_tail
            elif format_tail != curr_tail:
                msg = 'formatTail did not match between signature blocks'
                raise ValueError(msg)
            if format_length is None:
                format_length = protected['formatLength']
            elif format_length != protected['formatLength']:
                msg = 'formatLen did not match between signature blocks'
                raise ValueError(msg)
    message = text[0:format_length] + format_tail
    if hashlib.sha256(message).hexdigest() != digest:
        msg = 'Failed to match manifest digest to downloaded content'
        raise ValueError(msg)

    return True


def _setup_http_conn(url, cacert=None):
    """Prepare http connection object and return it."""
    target = urlparse.urlparse(url)
    conn = None

    if target.scheme == 'http':
        if 'http_proxy' in os.environ and need_proxy(target.hostname):
            proxy = urlparse.urlparse(os.environ['http_proxy'])
            conn = httplib.HTTPConnection(proxy.netloc)
            conn.set_tunnel(
                target.hostname,
                target.port if target.port else 80
            )
            conn.connect()
        else:
            conn = httplib.HTTPConnection(target.netloc)
    elif target.scheme == 'https':
        useproxy = False
        if 'https_proxy' in os.environ and need_proxy(target.hostname):
            proxy = urlparse.urlparse(os.environ['https_proxy'])
            useproxy = True
        try:
            ssl_context = ssl.create_default_context()
            if cacert is not None:
                ssl_context = ssl.create_default_context(cafile=cacert)
            if useproxy:
                conn = httplib.HTTPSConnection(
                    proxy.netloc,
                    context=ssl_context
                )
                conn.set_tunnel(
                    target.hostname,
                    target.port if target.port else 443
                )
                conn.connect()
            else:
                conn = httplib.HTTPSConnection(
                    target.netloc,
                    context=ssl_context
                )
        except AttributeError:
            if useproxy:
                conn = httplib.HTTPSConnection(
                    proxy.netloc,
                    key_file=None,
                    cert_file=cacert
                )
                conn.set_tunnel(
                    target.hostname,
                    target.port if target.port else 443
                )
                conn.connect()
            else:
                conn = httplib.HTTPSConnection(
                    target.netloc,
                    key_file=None,
                    cert_file=cacert
                )
    else:
        raise ValueError("Error, unknown protocol %s" % target.scheme)
    return conn


def _construct_image_metadata(manifest):
    """Perform introspection and analysis of docker manifest."""
    if manifest is None:
        raise ValueError('Invalid manifest')

    req_keys = ('schemaVersion', 'fsLayers', 'history', 'signatures')
    if any([x for x in req_keys if x not in manifest]):
        raise ValueError('Manifest in incorrect format')

    if manifest['schemaVersion'] != 1:
        raise ValueError('Incompatible manifest schema')

    if len(manifest['fsLayers']) != len(manifest['history']):
        raise ValueError('Manifest layer size mismatch')

    layers = {}
    no_parent = None
    for idx, layer in enumerate(manifest['history']):
        if 'v1Compatibility' not in layer:
            raise ValueError('Unknown layer format')
        layer_data = json.loads(layer['v1Compatibility'])
        layer_data['fsLayer'] = manifest['fsLayers'][idx]
        if 'parent' not in layer_data:
            if no_parent is not None:
                msg = 'Found more than one layer with no parent, exiting'
                raise ValueError(msg)
            no_parent = layer_data
        else:
            if layer_data['parent'] in layers:
                if layers[layer_data['parent']]['id'] == layer_data['id']:
                    # skip already-existing image
                    continue
                raise ValueError('Multiple inheritance from a layer, ' +
                                 'unsure how to proceed')
            layers[layer_data['parent']] = layer_data
        if 'id' not in layer_data:
            raise ValueError('Malformed layer, missing id')

    if no_parent is None:
        msg = 'Unable to find single layer wihtout parent, '
        msg += 'cannot identify terminal layer'
        raise ValueError(msg)

    # traverse graph and construct linked-list of layers
    curr = no_parent
    while curr is not None:
        if curr['id'] in layers:
            curr['child'] = layers[curr['id']]
            del layers[curr['id']]
            curr = curr['child']
        else:
            curr['child'] = None
            break

    return (no_parent, curr,)


class DockerV2Handle(object):
    """
    A class for fetching and unpacking docker registry (and dockerhub) images.
    This class is a client of the Docker Registry V2 protocol.
    """

    repo = None
    tag = None
    protocol = 'https'
    server = 'registry-1.docker.io'
    base_path = '/v2'
    cacert = None
    username = None
    password = None
    token = None
    allow_authenticated = True
    check_layer_checksums = True

    # excluding empty tar blobSum because python 2.6 throws an exception when
    # an open is attempted
    excludeBlobSums = [_EMPTY_TAR_SHA256]

    def __init__(self, imageIdent, options=None, updater=None):
        """
        Initialize an instance of the DockerV2 class.
        imageIdent is a tagged repo (e.g., ubuntu:14.04)
        options is a dictionary.  Valid options include:
            baseUrl to specify a URL other than dockerhub
            cacert to specify an approved signing authority
            username/password to specify a login
        """
        # attempt to parse image identifier
        try:
            self.repo, self.tag = imageIdent.strip().split(':', 1)
        except ValueError:
            if isinstance(imageIdent, str):
                msg = 'Invalid docker image identifier: %s' % imageIdent
                raise ValueError(msg)
            else:
                raise ValueError('Invalid type for docker image identifier')

        if options is None:
            options = {}
        if not isinstance(options, dict):
            raise ValueError('Invalid type for DockerV2 options')
        self.updater = updater

        if 'baseUrl' in options:
            base_url = options['baseUrl']
            protocol = None
            server = None
            base_path = None
            if base_url.find('http') >= 0:
                protocol = base_url.split(':')[0]
                index = base_url.find(':')
                base_url = base_url[index + 3:]
            if base_url.find('/') >= 0:
                index = base_url.find('/')
                base_path = base_url[index:]
                server = base_url[:index]
            else:
                server = base_url

            if protocol is None or len(protocol) == 0:
                protocol = 'https'

            if server is None or len(server) == 0:
                msg = 'unable to parse baseUrl, no server '
                msg += 'specified, should be like \n'
                msg += 'https://server.location/optionalBasePath'
                raise ValueError(msg)

            if base_path is None:
                base_path = '/v2'

            self.protocol = protocol
            self.server = server
            self.base_path = base_path
            self.private = False
            self.headers = {}

        if self.protocol == 'http':
            self.allow_authenticated = False

        self.url = '%s://%s' % (self.protocol, self.server)

        if self.repo.find('/') == -1 and self.server.endswith('docker.io'):
            self.repo = 'library/%s' % self.repo

        if 'cacert' in options:
            if not os.path.exists(options['cacert']):
                raise ValueError('specified cacert file does not exist: %s'
                                 % options['cacert'])

            self.cacert = options['cacert']

        if 'username' in options:
            self.username = options['username']
        if 'password' in options:
            self.password = options['password']

        if (self.password is not None and self.username is None) or \
                (self.username is not None and self.password is None):
            raise ValueError('if either username or password is specified, ' +
                             'both must be')

        # if self.allow_authenticated is False and self.username is not None:
        #    raise ValueError('authentication not allowed with the current ' +
        #                     'settings (make sure you are using https)')

        self.auth_method = 'token'
        if 'authMethod' in options:
            self.auth_method = options['authMethod']
        self.eldest = None
        self.youngest = None

    def get_eldest_layer(self):
        """Return base layer"""
        return self.eldest

    def log(self, state, message=''):
        """Write state/message to upstream status collector"""
        if self.updater is not None:
            self.updater.update_status(state, message)

    def exclude_layer(self, blobsum):
        """Prevent a layer from being downloaded/extracted/examined"""
        # TODO: add better verfication of the blobsum, potentially give other
        # routes to mask out a layer with this function
        if blobsum not in self.excludeBlobSums:
            self.excludeBlobSums.append(blobsum)

    def do_token_auth(self, auth_loc_str, creds=False):
        """
        Perform token authorization as in Docker registry v2 specification.
        :param auth_loc_str: from "WWW-Authenticate" response header
        Formatted as:
            <mode> realm=<authUrl>,service=<service>,scope=<scope>
            The mode will typically be "bearer", service and scope are the repo
            and capabilities being requested respectively.  For shifter, the
            scope will only be pull.

        """
        auth_data_str = None
        if self.allow_authenticated is False and self.username is not None:
            raise ValueError('authentication not allowed with the current ' +
                             'settings (make sure you are using https)')

        # TODO, figure out what mode was for
        (_, auth_data_str) = auth_loc_str.split(' ', 2)

        auth_data = {}
        for item in auth_data_str.split(','):
            (key, val) = item.split('=', 2)
            auth_data[key] = val.replace('"', '')

        auth_conn = _setup_http_conn(auth_data['realm'], self.cacert)
        if auth_conn is None:
            raise ValueError('Bad response from registry, ' +
                             'failed to get auth connection')

        headers = {}
        if creds and self.username is not None and self.password is not None:
            print "\nUsing Usernmae/Password: private set to True\n"
            self.private = True
            auth = '%s:%s' % (self.username, self.password)
            headers['Authorization'] = 'Basic %s' % base64.b64encode(auth)

        match_obj = re.match(r'(https?)://(.*?)(/.*)', auth_data['realm'])
        if match_obj is None:
            raise ValueError('Failed to parse authorization URL: %s'
                             % auth_data['realm'])
        path = match_obj.groups()[2]
        path = '%s?service=%s&scope=%s' \
               % (path, auth_data['service'], auth_data['scope'])
        auth_conn.request("GET", path, None, headers)
        resp = auth_conn.getresponse()

        if resp.status != 200:
            raise ValueError('Bad response getting token: %d', resp.status)
        if resp.getheader('content-type') != 'application/json':
            raise ValueError('Invalid response getting token, not json')

        auth_resp = json.loads(resp.read())
        self.token = auth_resp['token']

    def get_image_manifest(self, retrying=False):
        """
        Get the image manifest returns a dictionary object of the manifest.
        """
        conn = _setup_http_conn(self.url, self.cacert)
        if conn is None:
            return None

        self._get_auth_header()

        req_path = "/v2/%s/manifests/%s" % (self.repo, self.tag)
        conn.request("GET", req_path, None, self.headers)
        resp1 = conn.getresponse()

        if resp1.status == 401 and not retrying and \
                self.auth_method == 'token':
            # First try authenticating as public (no creds)
            self.do_token_auth(resp1.getheader('WWW-Authenticate'))
            try:
                return self.get_image_manifest(retrying=True)
            except:
                # Likely failed because it needs a cred, continue
                pass

            # So that must have failed, let's try with creds
            # TODO: If there aren't any creds it should just fail out
            # now because we are basically repeating the previous
            # attempt
            self.do_token_auth(resp1.getheader('WWW-Authenticate'),
                               creds=True)
            return self.get_image_manifest(retrying=True)
        if resp1.status != 200:
            msg = "Bad response from registry status=%d" % (resp1.status)
            raise ValueError(msg)
        expected_hash = resp1.getheader('docker-content-digest')
        content_len = int(resp1.getheader('content-length'))
        if expected_hash is None or len(expected_hash) == 0:
            raise ValueError("No docker-content-digest header found")
        expected_hash = expected_hash.split(':', 1)[1]
        data = resp1.read()
        if len(data) != content_len:
            memo = "Failed to read manifest: %d/%d bytes read" \
                   % (len(data), content_len)
            raise ValueError(memo)
        jdata = json.loads(data)

        # throws exceptions upon failure only
        _verify_manifest_signature(jdata, data, expected_hash)
        return jdata

    def examine_manifest(self, manifest):
        """Extract metadata from manifest."""
        self.log("PULLING", 'Constructing manifest')
        (eldest, youngest) = _construct_image_metadata(manifest)

        self.eldest = eldest
        self.youngest = youngest
        meta = youngest

        resp = {'id': meta['id']}
        if 'config' in meta:
            config = meta['config']
            if 'Env' in config:
                resp['env'] = config['Env']
            if 'WorkingDir' in config:
                if config['WorkingDir'] != '':
                    resp['workdir'] = config['WorkingDir']
            if 'Entrypoint' in config:
                resp['entrypoint'] = config['Entrypoint']
        resp['private'] = self.private
        return resp

    def pull_layers(self, manifest, cachedir):
        """Download layers to cachedir if they do not exist."""
        # TODO: don't rely on self.eldest to demonstrate that
        # examine_manifest has run
        if self.eldest is None:
            self.examine_manifest(manifest)
        layer = self.eldest
        while layer is not None:
            if layer['fsLayer']['blobSum'] in self.excludeBlobSums:
                layer = layer['child']
                continue

            memo = "Pulling layer %s" % layer['fsLayer']['blobSum']
            self.log("PULLING", memo)

            self.save_layer(layer['fsLayer']['blobSum'], cachedir)
            layer = layer['child']
        return True

    def _get_auth_header(self):
        """
        Helper function to generate the header.
        """
        if self.auth_method == 'token' and self.token is not None:
            self.headers = {'Authorization': 'Bearer %s' % self.token}
        elif self.auth_method == 'basic' and self.username is not None:
            auth = '%s:%s' % (self.username, self.password)
            self.headers['Authorization'] = 'Basic %s' % base64.b64encode(auth)
        return self.headers

    def save_layer(self, layer, cachedir='./'):
        """
        Save a layer and verify with the digest
        """
        path = "/v2/%s/blobs/%s" % (self.repo, layer)
        url = self.url
        while True:
            conn = _setup_http_conn(url, self.cacert)
            if conn is None:
                return None

            filename = '%s/%s.tar' % (cachedir, layer)

            if os.path.exists(filename):
                try:
                    return self.check_layer_checksum(layer, filename)
                except ValueError:
                    # there was a checksum mismatch, nuke the file
                    os.unlink(filename)

            conn.request("GET", path, None, self.headers)
            resp1 = conn.getresponse()
            location = resp1.getheader('location')
            if resp1.status == 200:
                break

            elif resp1.status == 401 and self.auth_method == 'token':
                self.do_token_auth(resp1.getheader('WWW-Authenticate'))
                continue
            elif location is not None:
                url = location
                match_obj = re.match(r'(https?)://(.*?)(/.*)', location)
                url = '%s://%s' % (match_obj.groups()[0],
                                   match_obj.groups()[1])
                path = match_obj.groups()[2]
            else:
                print 'ERROR: Getting layer recieved status: %d' % resp1.status
                return False
        maxlen = int(resp1.getheader('content-length'))
        nread = 0
        (out_fd, out_fn) = tempfile.mkstemp('.partial', layer, cachedir)
        out_fp = os.fdopen(out_fd, 'w')

        try:
            readsz = 4 * 1024 * 1024  # read 4MB chunks
            while nread < maxlen:
                # TODO find a way to timeout a failed read
                buff = resp1.read(readsz)
                if buff is None:
                    break

                out_fp.write(buff)
                nread += len(buff)
            out_fp.close()
            self.check_layer_checksum(layer, out_fn)
        except:
            os.unlink(out_fn)
            out_fp.close()
            raise

        os.rename(out_fn, filename)
        return True

    def check_layer_checksum(self, layer, filename):
        """Perform checksum calculation to exhaustively validate download."""
        if self.check_layer_checksums is False:
            return True

        (hash_type, value) = layer.split(':', 1)
        exec_name = '%s%s' % (hash_type, 'sum')
        process = Popen([exec_name, filename], stdout=PIPE)

        stdout = process.communicate()[0]
        checksum = stdout.split(' ', 1)[0]
        if checksum != value:
            raise ValueError("checksum mismatch, failure")
        return True

    def extract_docker_layers(self, base_path, base_layer, cachedir='./'):
        """Analyze files in docker layers and extract minimal set to base_path.
        """
        def filter_layer(layer_members, to_remove):
            """Remove members and members starting with to_remove from layer"""
            trailing = '/' if not to_remove.endswith('/') else ''
            prefix_to_remove = to_remove + trailing

            return [x for x in layer_members
                    if (not x.name == to_remove and
                        not x.name.startswith(prefix_to_remove))]

        layer_paths = []
        tar_file_refs = []
        layer = base_layer
        while layer is not None:
            if layer['fsLayer']['blobSum'] in self.excludeBlobSums:
                layer = layer['child']
                continue

            tfname = '%s.tar' % layer['fsLayer']['blobSum']
            tfname = os.path.join(cachedir, tfname)
            tfp = tarfile.open(tfname, 'r:gz')
            tar_file_refs.append(tfp)

            # get directory of tar contents
            members = tfp.getmembers()

            # remove all illegal files
            members = filter_layer(members, 'dev/')
            members = filter_layer(members, '/')
            members = [x for x in members if not x.name.find('..') >= 0]

            # find all whiteouts
            whiteouts = [x for x in members
                         if x.name.find('/.wh.') >= 0 or
                         x.name.startswith('.wh.')]

            # remove the whiteout tags from this layer
            for wh_ in whiteouts:
                members.remove(wh_)

            # remove the whiteout targets from all ancestral layers
            for idx, ancs_layer in enumerate(layer_paths):
                for wh_ in whiteouts:
                    path = wh_.name.replace('/.wh.', '/')
                    if path.startswith('.wh.'):
                        path = path[4:]
                    ancs_layer_iter = (x for x in ancs_layer if x.name == path)
                    ancs_member = next(ancs_layer_iter, None)
                    if ancs_member:
                        ancs_layer = filter_layer(ancs_layer, path)
                layer_paths[idx] = ancs_layer

            # remove identical paths (not dirs) from all ancestral layers
            notdirs = [x.name for x in members if not x.isdir()]
            for idx, ancs_layer in enumerate(layer_paths):
                # look for files that were directories
                filter = [x.name for x in ancs_layer if x.isdir() and
                          x.name in notdirs]
                # And then filter them out and any files or subdirs
                for f in filter:
                    ancs_layer = [x for x in ancs_layer if x.name != f and
                                  not x.name.startswith(f + '/')]
                ancs_layer = [x for x in ancs_layer if x.name not in notdirs]
                layer_paths[idx] = ancs_layer

            # push this layer into the collection
            layer_paths.append(members)

            layer = layer['child']

        # extract the selected files
        layer_idx = 0
        layer = base_layer
        while layer is not None:
            if layer['fsLayer']['blobSum'] in self.excludeBlobSums:
                layer = layer['child']
                continue

            tfp = tar_file_refs[layer_idx]
            members = layer_paths[layer_idx]
            tfp.extractall(path=base_path, members=members)
            # We need to make sure everything is writeable by the user so
            # subsequent layers can do overwrites
            for f in members:
                path = base_path + '/' + f.name
                mode = f.mode
                if not stat.S_ISLNK(mode) and (f.mode & stat.S_IWUSR) == 0:
                    os.chmod(path, mode | stat.S_IWUSR)

            layer_idx += 1
            layer = layer['child']

        for tfp in tar_file_refs:
            tfp.close()

        # fix permissions on the extracted files
        cmd = ['chmod', '-R', 'a+rX,u+w', base_path]
        pfp = Popen(cmd)
        pfp.communicate()


# Deprecated: Just use the object above
def pull_image(options, repo, tag, cachedir='./', expanddir='./'):
    """
    Uber function to pull the manifest, layers, and extract the layers
    """
    if options is None:
        options = {}
    if 'username' not in options:
        options['username'] = None
    if 'password' not in options:
        options['password'] = None
    if 'baseUrl' not in options:
        options['baseUrl'] = 'https://registry-1.docker.io'
    imageident = '%s:%s' % (repo, tag)
    handle = DockerV2Handle(imageident, options)

    manifest = handle.get_image_manifest()
    (eldest, youngest) = _construct_image_metadata(manifest)
    layer = eldest
    while layer is not None:
        handle.save_layer(layer['fsLayer']['blobSum'], cachedir)
        layer = layer['child']

    layer = eldest
    meta = youngest
    resp = {'id': meta['id']}
    expandedpath = os.path.join(expanddir, str(meta['id']))
    resp['expandedpath'] = expandedpath
    if 'config' in meta:
        config = meta['config']
        if 'Env' in config:
            resp['env'] = config['Env']
        if 'WorkingDir' in config:
            resp['workdir'] = config['WorkingDir']
        if 'Entrypoint' in config:
            resp['entrypoint'] = config['Entrypoint']

    if not os.path.exists(expandedpath):
        os.mkdir(expandedpath)

    handle.extract_docker_layers(expandedpath, layer, cachedir=cachedir)
    return resp


def main():
    """Harness for manual testing."""
    cache_dir = os.environ['TMPDIR']
    pull_image({'baseUrl': 'https://registry-1.docker.io'}, 'scanon/shanetest',
               'latest', cachedir=cache_dir, expanddir=cache_dir)


if __name__ == '__main__':
    main()
