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
This module provides the worker function for the image gateway.
"""

import os
import shutil
import sys
import subprocess
import logging
import tempfile
from multiprocessing import Queue
from multiprocessing.pool import ThreadPool
from time import time
from shifter_imagegw import converters, transfer
from shifter_imagegw.dockerv2 import DockerV2Handle as DockerV2
from shifter_imagegw.dockerv2_ext import DockerV2ext


class Updater(object):
    """
    This is a helper class to update the status for the request.
    """
    def __init__(self, ident, update_method):
        """ init the updater. """
        self.ident = ident
        self.update_method = update_method

    def update_status(self, state, message, response=None):
        """ update the status including the heartbeat and message """
        if self.update_method is not None:
            metadata = {'heartbeat': time(),
                        'message': message,
                        'response': response}
            self.update_method(ident=self.ident, state=state, meta=metadata)

    def failed(self, e):
        if self.update_method is not None:
            metadata = {'heartbeat': time(),
                        'message': "Operation Failed",
                        'response': {}}
            self.update_method(ident=self.ident, state="FAILURE",
                               meta=metadata)


class WorkerThreads(object):
    def __init__(self, conf, threads=1):
        """
        Initialize the thread pool and queues.
        """
        self.pools = ThreadPool(processes=threads)
        self.updater_queue = Queue()
        self.conf = conf
        if 'CacheDirectory' in conf:
            if not os.path.exists(conf['CacheDirectory']):
                os.mkdir(conf['CacheDirectory'])
        if 'ExpandDirectory' in conf:
            if not os.path.exists(conf['ExpandDirectory']):
                os.mkdir(conf['ExpandDirectory'])
        if 'ConvertOptions' in conf and \
           not isinstance(conf['ConverterOptions'], dict):
            raise ValueError('ConverterOptions must be a dictionary')

    def get_updater_queue(self):
        return self.updater_queue

    def updater(self, ident, state, meta):
        """
        Updater function: This just post a message to a queue.
        """
        self.updater_queue.put({'id': ident, 'state': state, 'meta': meta})

    def pull(self, request, updater):
        try:
            req = ImageRequest(self.conf, request, updater)
            req.pull()
        except Exception as err:
            resp = {'error_type': str(type(err)),
                    'message': str(err)}

            updater.update_status('FAILURE', 'FAILURE', response=resp)

    def expire(self, request, updater):
        try:
            req = ImageRequest(self.conf, request, updater)
            req.remove_image()
        except Exception as err:
            resp = {'error_type': str(type(err)),
                    'message': str(err)}
            updater.update_status('FAILURE', 'FAILURE', response=resp)

    def wrkimport(self, request, updater):
        try:
            req = ImageRequest(self.conf, request, updater)
            req.img_import()
        except Exception as err:
            resp = {'error_type': str(type(err)),
                    'message': str(err)}
            updater.update_status('FAILURE', 'FAILURE', response=resp)

    def dopull(self, ident, request):
        """
        Kick off a pull operation.
        """
        updater = Updater(ident, self.updater)
        self.pools.apply_async(self.pull, [request, updater],
                               {}, None, updater.failed)

    def doexpire(self, ident, request):
        updater = Updater(ident, self.updater)
        self.pools.apply_async(self.expire, [request, updater],
                               {}, None, updater.failed)

    def dowrkimport(self, ident, request):
        logging.debug("wrkimport starting")
        updater = Updater(ident, self.updater)
        self.pools.apply_async(self.wrkimport, [request, updater],
                               {}, None, updater.failed)


class ImageRequest(object):
    def __init__(self, conf, request, updater):
        self.conf = conf
        self.updater = updater
        self.fmt = self.conf['DefaultImageFormat']
        if 'format' in request:
            self.fmt = request['format']
        self.system = request['system']
        if self.system not in self.conf['Platforms']:
            raise KeyError(f'{self.system} is not in the configuration')
        self.sysconf = self.conf['Platforms'][self.system]
        self.tag = request.get('tag')
        self.id = request.get('id')
        self.meta = None
        self.meta_only = False
        self.import_image = False
        self.metafile = None
        self.expandedpath = None
        self.imagefile = None
        self.filepath = request.get('filepath')
        self.session = request.get('session')
        self.tokens = None
        self.user = None
        if self.session:
            self.user = self.session.get('user')
            self.tokens = self.session.get('tokens')

        self.userACL = request.get('userACL')
        self.groupACL = request.get('groupACL')

    def _get_cacert(self, location):
        """ Private method to get the cert location """
        params = self.conf['Locations'][location]
        cacert = None
        currdir = os.getcwd()
        if 'sslcacert' in params:
            if params['sslcacert'].startswith('/'):
                cacert = params['sslcacert']
            else:
                cacert = f"{currdir}/{params['sslcacert']}"
            if not os.path.exists(cacert):
                raise OSError(f'{cacert} does not exist')
        return cacert

    def _pull_dockerv2(self, location, repo, tag):
        """ Private method to pull a docker images. """
        cdir = self.conf['CacheDirectory']
        edir = self.conf['ExpandDirectory']
        params = self.conf['Locations'][location]
        cacert = self._get_cacert(location)

        url = f'https://{location}'
        if 'url' in params:
            url = params['url']
        try:
            options = {}
            if cacert is not None:
                options['cacert'] = cacert
            options['baseUrl'] = url
            if 'authMethod' in params:
                options['authMethod'] = params['authMethod']

            if (self.tokens):
                if location in self.tokens:
                    userpass = self.tokens[location]
                    options['username'] = userpass.split(':')[0]
                    options['password'] = ''.join(userpass.split(':')[1:])
                elif ('default' in self.tokens):
                    userpass = self.tokens['default']
                    options['username'] = userpass.split(':')[0]
                    options['password'] = ''.join(userpass.split(':')[1:])
            imgid = f'{repo}:{tag}'
            if self.sysconf.get('use_external'):
                options['policy_file'] = self.sysconf.get("policy_file")
                dock = DockerV2ext(imgid, options, updater=self.updater,
                                   cachedir=cdir)
            else:
                dock = DockerV2(imgid, options, updater=self.updater,
                                cachedir=cdir)
            self.updater.update_status("PULLING", 'Getting manifest')
            self.meta = dock.examine_manifest()
            # Get the ID
            self.id = str(self.meta['id'])

            if self._check_image():
                return True

            dock.pull_layers()

            self.expandedpath = tempfile.mkdtemp(suffix='extract',
                                                 prefix=self.id,
                                                 dir=edir)

            self.updater.update_status("PULLING", 'Extracting Layers')
            dock.extract_docker_layers(self.expandedpath)
            return True
        except Exception:
            logging.warn(sys.exc_info()[1])
            raise

        return False

    def _pull_image(self):
        """
        pull the image down and extract the contents

        Returns True on success
        """
        params = None
        rtype = None

        # See if there is a location specified
        location = self.conf['DefaultImageLocation']
        tag = self.tag
        if tag.find('/') > 0:
            parts = tag.split('/')
            if parts[0] in self.conf['Locations']:
                # This is a location
                location = parts[0]
                tag = '/'.join(parts[1:])

        parts = tag.split(':')
        if len(parts) == 2:
            (repo, tag) = parts
        else:
            raise OSError(f'Unable to parse tag {self.tag}')
        logging.debug("doing image pull for "
                      f"loc={location} repo={repo} tag={tag}")

        if location in self.conf['Locations']:
            params = self.conf['Locations'][location]
            rtype = params['remotetype']
        else:
            raise KeyError(f'{location} not found in configuration')

        if rtype == 'dockerv2':
            return self._pull_dockerv2(location, repo, tag)
        elif rtype == 'dockerhub':
            logging.warning("Use of depcreated dockerhub type")
            msg = 'dockerhub type is depcreated. Use dockerv2'
            raise NotImplementedError(msg)
        else:
            raise NotImplementedError(f"Unsupported remote type {rtype}")
        return False

    def _examine_image(self):
        """
        examine the image

        Returns True on success
        """

        if 'examiner' in self.conf:
            examiner = self.conf['examiner']
            retcode = subprocess.call([examiner, self.expandedpath,
                                      self.id])
            if retcode != 0:
                return False

        return True

    def _convert_image(self):
        """
        Convert the image to the required format for the target system

        Returns True on success
        """
        edir = self.conf['ExpandDirectory']
        if 'ConverterOptions' in self.conf:
            opts = self.conf['ConverterOptions']
        else:
            opts = None

        imagefile = os.path.join(edir, f'{self.id}.{self.fmt}')
        self.imagefile = imagefile

        status = converters.convert(self.fmt,
                                    self.expandedpath,
                                    imagefile, options=opts)
        return status

    def _write_metadata(self):
        """
        Write out the metadata file

        Returns True on success
        """
        self.meta['userACL'] = self.userACL
        self.meta['groupACL'] = self.groupACL

        edir = self.conf['ExpandDirectory']

        # initially write metadata to tempfile
        (fdesc, metafile) = tempfile.mkstemp(prefix=self.id,
                                             suffix='meta',
                                             dir=edir)
        os.close(fdesc)
        self.metafile = metafile

        status = converters.writemeta(self.fmt, self.meta, metafile)

        # after success move to final name
        final_metafile = os.path.join(edir, f'{self.id}.meta')
        shutil.move(metafile, final_metafile)
        self.metafile = final_metafile

        return status

    def _check_image(self):
        """
        Checks if the target image is on the target system

        Returns True on success
        """
        image_filename = f"{self.id}.{self.fmt}"
        image_metadata = f"{self.id}.meta"

        return transfer.imagevalid(self.sysconf, image_filename,
                                   image_metadata, logging)

    def _transfer_image(self):
        """
        Transfers the image to the target system based on the configuration.

        Returns True on success
        """
        if self.meta_only:
            return transfer.transfer(self.sysconf, None,
                                     self.metafile, logging)
        else:
            if not self.import_image:
                return transfer.transfer(self.sysconf,
                                         self.imagefile,
                                         self.metafile,
                                         logging, self.import_image)
            else:
                return transfer.transfer(self.sysconf,
                                         self.filepath,
                                         self.metafile,
                                         logging,
                                         self.import_image,
                                         self.imagefile)

    def remove_image(self):
        """
        Remove the image to the target system based on the configuration.

        Returns True on success
        """
        logging.debug(f"do expire system={self.system} tag={self.tag}")
        self.updater.update_status('EXPIRING', 'EXPIRING')

        imagefile = self.id + '.' + self.fmt
        meta = self.id + '.meta'
        if self.metafile:
            meta = self.metafile
        if transfer.remove(self.sysconf, imagefile, meta, logging):
            self.updater.update_status('EXPIRED', 'EXPIRED')
        else:
            logging.warn("Worker: Expire failed")
            raise OSError('Expire failed')

    def _cleanup_temporary(self):
        """
        Helper function to cleanup any temporary files or directories.
        """
        if not self.import_image:
            items = (self.expandedpath,
                     self.imagefile,
                     self.metafile)
        else:
            items = (self.expandedpath,
                     self.metafile)
        for cleanitem in items:
            if cleanitem is None:
                continue
            if isinstance(cleanitem, str):
                cleanitem = str(cleanitem)

            if not isinstance(cleanitem, str):
                msg = f"Invalid type for {cleanitem},{type(cleanitem)}"
                raise ValueError(msg)
            if cleanitem == '' or cleanitem == '/':
                msg = f"Invalid value for {cleanitem},{type(cleanitem)}"
                raise ValueError(msg)
            if not cleanitem.startswith(self.conf['ExpandDirectory']):
                msg = f"Invalid location for {cleanitem},{type(cleanitem)}"
                raise ValueError(msg)
            if os.path.exists(cleanitem):
                logging.info(f"Worker: removing {cleanitem}")
                try:
                    subprocess.call(['chmod', '-R', 'u+w', cleanitem])
                    if os.path.isdir(cleanitem):
                        shutil.rmtree(cleanitem, ignore_errors=True)
                    else:
                        os.unlink(cleanitem)
                except Exception:
                    logging.error("Worker: caught exception while trying to "
                                  f"clean up {cleanitem}.")

    def pull(self):
        """
        Main task to do the full workflow of pulling an image and transferring
        it
        """
        tag = self.tag
        logging.debug(f"dopull system={self.system} tag={tag}")
        try:
            # Step 1 - Do the pull
            self.updater.update_status('PULLING', 'PULLING')
            logging.debug(self.tag)
            if not self._pull_image():
                logging.info("Worker: Pull failed")
                raise OSError('Pull failed')

            if not self.meta:
                raise OSError('Metadata not populated')

            if not self._check_image():
                # Step 2 - Check the image
                self.updater.update_status('EXAMINATION', 'Examining image')
                logging.debug(f"Worker: examining image {tag}")
                if not self._examine_image():
                    raise OSError('Examine failed')
                # Step 3 - Convert
                self.updater.update_status('CONVERSION', 'Converting image')
                logging.debug(f"Worker: converting image {tag}")
                if not self._convert_image():
                    raise OSError('Conversion failed')
                if not self._write_metadata():
                    raise OSError('Metadata creation failed')
                # Step 4 - TRANSFER
                self.updater.update_status('TRANSFER', 'Transferring image')
                logging.debug(f"Worker: transferring image {tag}")
                if not self._transfer_image():
                    raise OSError('Transfer failed')
            else:
                self.meta_only = True
                self.meta['meta_only'] = True
                logging.debug(f"Updating metdata for {self.tag}")
                if not self._write_metadata():
                    raise OSError('Metadata creation failed')
                self.updater.update_status('TRANSFER', 'Transferring metadata')
                logging.debug(f"Worker: transferring metadata {self.tag}")
                if not self._transfer_image():
                    raise OSError('Transfer failed')

            # Done
            self.updater.update_status('READY', 'Image ready',
                                       response=self.meta)
            self._cleanup_temporary()
            return self.meta

        except Exception:
            logging.error("ERROR: dopull failed "
                          f"system={self.system} tag={self.tag}")
            print(sys.exc_info()[1])
            self.updater.update_status('FAILURE', 'FAILED')

            # TODO: add a debugging flag and only disable cleanup if debugging
            self._cleanup_temporary()
            raise

    def img_import(self):
        """
        Task to do the full workflow of copying an image and processing it
        """
        tag = self.tag
        self.import_image = True
        logging.debug(f"img_import system={self.system} tag={tag}")
        try:
            # Step 0 - Check if path is valid
            if not transfer.check_file(self.filepath,
                                       self.sysconf,
                                       logging,
                                       import_image=self.import_image):
                raise OSError('Path not valid')
            # Step 1 - Calculate the hash of the file
            logging.debug("starting import hashing")
            self.updater.update_status('HASHING', 'HASHING')
            self.id = transfer.hash_file(self.filepath,
                                         self.sysconf, logging)
            # Step 2 - Populate the metadata file
            logging.debug("starting writing metadata")
            # if not self.meta:
            #     raise OSError('Metadata not populated')
            self.meta = {
                'format': self.fmt,
                'user': self.user
            }
            if not self._write_metadata():
                logging.info("Writing metadata")
                raise OSError('Metadata creation failed')
            # Step 3 - Copy image and meta file from user space to shifter area
            logging.debug("starting transfer")
            imgfile = self.id + '.' + self.fmt
            self.imagefile = imgfile
            self.meta['id'] = self.id
            self.updater.update_status('TRANSFER', 'TRANSFER')
            if not self._transfer_image():
                logging.warn("Worker: Import copy failed")
                raise OSError("Import copy failed")

            # Done
            self.updater.update_status('READY', 'Image ready',
                                       response=self.meta)
            self._cleanup_temporary()
            return self.meta

        except Exception:
            logging.error("ERROR: img_import failed "
                          f"system={self.system} tag={self.tag}")
            print(sys.exc_info()[1])
            self.updater.update_status('FAILURE', 'FAILED')

            # TODO: add a debugging flag and only disable cleanup if debugging
            self._cleanup_temporary()
            raise
