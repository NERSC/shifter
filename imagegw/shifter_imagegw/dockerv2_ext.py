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

import json
import os
import stat
from subprocess import Popen, PIPE
import base64
import tempfile
import shutil


class DockerV2ext(object):
    """
    A class for fetching and unpacking docker registry (and dockerhub) images.
    This class is a client of the Docker Registry V2 protocol.
    """

    def __init__(self, imageIdent, options={}, updater=None, cachedir=None):
        """
        Initialize an instance of the DockerV2 class.
        imageIdent is a tagged repo (e.g., ubuntu:14.04)
        options is a dictionary.  Valid options include:
            baseUrl to specify a URL other than dockerhub
            username/password to specify a login
        """
        # attempt to parse image identifier
        self.imageId = imageIdent
        self.options = options
        self.updater = updater
        self.tokens = None
        self.policy_file = options.get('policy_file')
        registry = 'index.docker.io'
        if options and 'baseUrl' in options:
            registry = options['baseUrl']
            if registry.startswith('https:'):
                registry = registry[8:]
            elif registry.startswith('http:'):
                registry = registry[7:]
            registry = registry.rstrip('/')
        self.registry = registry
        self.url = 'docker://%s/%s' % (registry, imageIdent)
        self.private = False
        self.meta = self._inspect()
        self.meta['id'] = self.meta['Digest'].replace('sha256:', '')
        self.id = self.meta['id']
        self.cache_dir = cachedir
        self.pulled = False
        self.image_dir = None

    def get_eldest_layer(self):
        """Return base layer"""
        return None

    def log(self, state, message=''):
        """Write state/message to upstream status collector"""
        if self.updater is not None:
            self.updater.update_status(state, message)

    def _auth_file(self):
        if 'username' not in self.options:
            raise OSError("No authentication")
        user = self.options['username']
        pwd = self.options['password']

        token = base64.b64encode('%s:%s' % (user, pwd))
        afile = tempfile.mkstemp()[1]
        auth = {'auths': {
                    self.registry: {'auth': token}
                    }
                }
        with open(afile, 'w') as f:
            f.write(json.dumps(auth))
        self.auth_file = afile

    def _inspect(self):
        """
        Inspect image
        """
        self.log("PULLING", 'Inspecting Image')
        cmd = ['skopeo']
        if self.policy_file:
            cmd.extend(['--policy', self.policy_file])
        cmd.extend(['inspect', self.url])
        process = Popen(cmd, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()
        if 'authentication required' not in stderr and 'Forbidden' not in stderr:
            if process.returncode:
                raise OSError("Skopeo inspect failed")
            return json.loads(stdout)

        # Private Image
        self.log("PULLING", 'Inspecting Image with auth')
        self._auth_file()
        self.private = True
        cmd = ['skopeo']
        if self.policy_file:
            cmd.extend(['--policy', self.policy_file])
        cmd.extend(['inspect',  '--authfile', self.auth_file])
        cmd.extend([self.url])
        process = Popen(cmd, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()
        return json.loads(stdout)

    def _validate(self, idir):
        """
        Validate image pull
        """
        self.log("PULLING", 'Validating Image')
        cmd = ['oci-image-tool', 'validate', '--type',
               'image', idir]
        process = Popen(cmd, stdout=PIPE)
        stdout = process.communicate()[0]
        if process.returncode:
            raise OSError("Validation failed")
        return stdout

    def get_image_manifest(self, retrying=False):
        """
        Get the image manifest returns a dictionary object of the manifest.
        """
        return self.meta

    def examine_manifest(self):
        """Extract metadata from manifest."""
        self.log("PULLING", 'Constructing manifest')

        self.pull_layers()
        # Read manifest
        # with open(self.manifest_file) as f:
        #     data = f.read()
        #     manifest = json.loads(data)

        conf = self.manifest['config']['digest']
        meta = self._read_json_hash(conf)

        resp = {'id': self.id}
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

    def _read_json_hash(self, hash_name):
        hashalgo, hashid = hash_name.split(':')
        fn = os.path.join(self.cache_dir, hashalgo, hashid)
        with open(fn) as f:
            data = f.read()

        return json.loads(data)

    def pull_layers(self):
        """
        This is a no-op since we would have pulled it in the
        examine manifest
        """
        if self.pulled:
            return True
        self.log("PULLING", 'Pulling layers')

        outdir = os.path.join(self.cache_dir, self.id)
        cmd = ['skopeo']
        if self.policy_file:
            cmd.extend(['--policy', self.policy_file])
        cmd.extend(['copy', '--dest-shared-blob-dir', self.cache_dir])
        if self.private:
            cmd.extend(['--authfile', self.auth_file])
        cmd.extend([self.url, 'oci://%s:image' % outdir])
        process = Popen(cmd, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()

        # Read index file
        with open(os.path.join(outdir, 'index.json')) as f:
            data = json.loads(f.read())
        digest = data['manifests'][0]['digest']
        # hashalgo, hashid = data['manifests'][0]['digest'].split(':')
        # m_file = os.path.join(self.cache_dir, hashalgo, hashid)
        # self.manifest_file = m_file
        self.manifest = self._read_json_hash(digest)

        blobs = os.path.join(outdir, "blobs", "sha256")
        if not os.path.exists(blobs):
            os.symlink(os.path.join(self.cache_dir, 'sha256'),
                       blobs)
        self._validate(outdir)

        self.image_dir = outdir
        self.pulled = True
        return True

    def extract_docker_layers(self, base_path):
        """Analyze files in docker layers and extract minimal set to base_path.
        """
        self.log("PULLING", 'Extracting image')
        idir = base_path + '.image'
        cmd = ['umoci', 'unpack', '--rootless', '--image',
               self.image_dir + ':image', idir]
        process = Popen(cmd, stdout=PIPE)
        process.communicate()[0]
        if process.returncode:
            return False
        rootfs = os.path.join(idir, 'rootfs')
        os.rmdir(base_path)
        perm = os.stat(rootfs).st_mode
        os.chmod(rootfs, perm|stat.S_IEXEC|stat.S_IWRITE|stat.S_IREAD)
        os.rename(rootfs, base_path)
        shutil.rmtree(idir)
        return True
