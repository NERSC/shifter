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
import subprocess
import logging
import tempfile
from multiprocessing import Queue
from multiprocessing.pool import ThreadPool
from time import time
from shifter_imagegw import converters, transfer
from shifter_imagegw.dockerv2_ext import DockerV2ext
from shifter_imagegw.config import Config
from shifter_imagegw.models import Session


class Updater(object):
    """
    This is a helper class to update the status for the request.
    """
    def __init__(self, ident: str = None, update_method=None):
        """ init the updater. """
        self.ident = ident
        self.update_method = update_method

    def update_status(self, state: str, message: str, response=None):
        """ update the status including the heartbeat and message """
        if self.update_method:
            metadata = {'heartbeat': time(),
                        'message': message,
                        'response': response}
            self.update_method(ident=self.ident, state=state, meta=metadata)

    def failed(self, e):
        logging.error(f"Failure: {e}")
        if self.update_method:
            metadata = {'heartbeat': time(),
                        'message': "Operation Failed",
                        'response': {}}
            self.update_method(ident=self.ident, state="FAILURE",
                               meta=metadata)


class WorkerThreads(object):
    """
    This class handles creating a thread pool and
    submitting async work to these threads.
    """

    def __init__(self, conf, threads=1):
        """
        Initialize the thread pool and queues.
        """
        self.pools = ThreadPool(processes=threads)
        self.updater_queue = Queue()
        self.conf = conf
        if not os.path.exists(conf.CacheDirectory):
            os.mkdir(conf.CacheDirectory)
        if not os.path.exists(conf.ExpandDirectory):
            os.mkdir(conf.ExpandDirectory)

    def get_updater_queue(self):
        return self.updater_queue

    def updater(self, ident, state, meta):
        """
        Updater function: This just post a message to a queue.
        """
        self.updater_queue.put({'id': ident, 'state': state, 'meta': meta})

    def submit(self, req: object):
        # We have to override the updater method here since we
        # didn't have the method when it was initialized.
        req.updater.update_method = self.updater
        try:
            self.pools.apply_async(req.run, [], {}, None, req.updater.failed)
        except Exception as ex:
            logging.error(str(ex))
            raise ex


class AsyncRequest(object):
    """
    Basea async request class
    """
    clean_items = []

    def _null_func(self, **kwargs):
        pass

    def _cleanup_temporary(self):
        """
        Helper function to cleanup any temporary files or directories.
        """
        for cleanitem in self.clean_items:
            logging.debug(f"Cleaning {cleanitem}")
            if cleanitem is None:
                continue

            if not isinstance(cleanitem, str):
                err_msg = f'Invalid type for {cleanitem},{type(cleanitem)}'
                raise ValueError(err_msg)
            if cleanitem == '' or cleanitem == '/':
                raise ValueError(f'Invalid value for {cleanitem}: {cleanitem}')
            if not cleanitem.startswith(self.conf.ExpandDirectory):
                err_msg = f'Invalid location for {cleanitem}: {cleanitem}'
                raise ValueError(err_msg)
            if os.path.exists(cleanitem):
                logging.info(f"Worker: removing {cleanitem}")
                try:
                    subprocess.call(['chmod', '-R', 'u+w', cleanitem])
                    if os.path.isdir(cleanitem):
                        logging.debug(f"rmtree on {cleanitem}")
                        shutil.rmtree(cleanitem, ignore_errors=True)
                    else:
                        os.unlink(cleanitem)
                except Exception:
                    logging.error("Worker: caught exception while trying to "
                                  f"clean up {cleanitem}")

    def _write_metadata(self):
        """
        Write out the metadata file

        Returns True on success
        """
        self.meta['userACL'] = self.userACL
        self.meta['groupACL'] = self.groupACL

        edir = self.conf.ExpandDirectory

        # initially write metadata to tempfile
        (fdesc, metafile) = tempfile.mkstemp(prefix=self.id,
                                             suffix='meta',
                                             dir=edir)
        os.close(fdesc)
        self.metafile = metafile
        self.clean_items.append(self.metafile)

        status = converters.writemeta(self.fmt, self.meta, metafile)

        # after success move to final name
        final_metafile = os.path.join(edir, f'{self.id}.meta')
        shutil.move(metafile, final_metafile)
        self.metafile = final_metafile

        return status


class PullRequest(AsyncRequest):
    """
    Class to process a pull request.  This handles the entire
    workflow from pulling, converting and transferring
    """
    def __init__(self,
                 conf: Config,
                 system: str,
                 tag: str,
                 ident: str,
                 session: Session,
                 useracl=None,
                 groupacl=None):
        self.conf = conf
        self.fmt = self.conf.DefaultImageFormat
        self.system = system
        self.sysconf = self.conf.Platforms[self.system]
        self.tag = tag
        self.ident = ident
        self.id = None
        self.meta = None
        self.meta_only = False
        self.metafile = None
        self.expandedpath = None
        self.imagefile = None
        self.session = session
        self.tokens = None
        self.user = None
        if self.session:
            self.user = self.session.user
            self.tokens = self.session.tokens
        self.userACL = useracl
        self.groupACL = groupacl
        self.updater = Updater(ident, self._null_func)

    def _pull_dockerv2(self, location, repo, tag):
        """ Private method to pull a docker images. """
        cdir = self.conf.CacheDirectory
        edir = self.conf.ExpandDirectory
        loc = self.conf.Locations[location]
        username = None
        password = None

        url = loc.url if loc.url else f'https://{location}'
        if self.tokens:
            if location in self.tokens:
                userpass = self.tokens[location]
            elif 'default' in self.tokens:
                userpass = self.tokens['default']
            username, password = userpass.split(":", maxsplit=1)
        imgid = f'{repo}:{tag}'

        try:
            dock = DockerV2ext(imgid,
                               updater=self.updater,
                               baseurl=url,
                               username=username,
                               password=password,
                               policy_file=self.sysconf.policy_file,
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
            self.clean_items.append(self.expandedpath)

            self.updater.update_status("PULLING", 'Extracting Layers')
            dock.extract_docker_layers(self.expandedpath)
            return True
        except Exception as e:
            logging.warning(str(e))
            raise e

    def _pull_image(self):
        """
        pull the image down and extract the contents

        Returns True on success
        """
        # See if there is a location specified
        location = self.conf.DefaultImageLocation
        tag = self.tag
        if tag.find('/') > 0:
            parts = tag.split('/')
            if parts[0] in self.conf.Locations:
                # This is a location
                location = parts[0]
                tag = '/'.join(parts[1:])

        parts = tag.split(':')
        if len(parts) == 2:
            (repo, tag) = parts
        else:
            raise OSError(f'Unable to parse tag {self.tag}')
        logging.debug(f"Doing image pull for repo={repo} tag={tag}")

        return self._pull_dockerv2(location, repo, tag)

    def _examine_image(self):
        """
        examine the image

        Returns True on success
        """

        if self.conf.examiner:
            examiner = self.conf.examiner
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
        edir = self.conf.ExpandDirectory
        if self.conf.ConverterOptions:
            opts = self.conf.ConverterOptions
        else:
            opts = None

        imagefile = os.path.join(edir, f'{self.id}.{self.fmt}')
        self.imagefile = imagefile
        self.clean_items.append(self.imagefile)

        status = converters.convert(self.fmt,
                                    self.expandedpath,
                                    imagefile, options=opts)
        return status

    def _check_image(self):
        """
        Checks if the target image is on the target system

        Returns True on success
        """
        image_filename = f"{self.id}.{self.fmt}"
        image_metadata = f"{self.id}.meta"

        return transfer.imagevalid(self.sysconf, image_filename,
                                   image_metadata)

    def _transfer_image(self):
        """
        Transfers the image to the target system based on the configuration.

        Returns True on success
        """
        if self.meta_only:
            return transfer.transfer(self.sysconf, None,
                                     self.metafile)
        else:
            return transfer.transfer(self.sysconf,
                                     self.imagefile,
                                     self.metafile)

    def run(self):
        """
        Main task to do the full workflow of pulling an image and transferring
        it
        """
        tag = self.tag
        try:
            # Step 1 - Do the pull
            self.updater.update_status('PULLING', 'PULLING')
            logging.debug(self.tag)
            if not self._pull_image():
                logging.warning("Worker: Pull failed")
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
                logging.debug(F"Worker: transferring image {tag}")
                self._transfer_image()
                # if not self._transfer_image():
                #     raise OSError('Transfer failed')
            else:
                self.meta_only = True
                self.meta['meta_only'] = True
                logging.debug(f"Updating metdata for {self.tag}")
                if not self._write_metadata():
                    raise OSError('Metadata creation failed')
                self.updater.update_status('TRANSFER', 'Transferring metadata')
                logging.debug(f"Worker: transferring metadata {self.tag}")
                self._transfer_image()
                # if not self._transfer_image():
                #     raise OSError('Transfer failed')

            # Done
            self.updater.update_status('READY', 'Image ready',
                                       response=self.meta)
            self._cleanup_temporary()
            return self.meta

        except Exception as e:
            err_msg = f"ERROR: Dopull failed sys={self.system} tag={self.tag}"
            logging.error(err_msg)
            self.updater.update_status('FAILURE', 'FAILED')

            # TODO: add a debugging flag and only disable cleanup if debugging
            self._cleanup_temporary()
            logging.error(f"{str(e)}")
            raise e


class ImportRequest(AsyncRequest):
    def __init__(self,
                 conf: Config,
                 system: str,
                 tag: str,
                 id: str,
                 session: Session,
                 filepath: str,
                 useracl=None,
                 groupacl=None):

        self.conf = conf
        self.fmt = conf.DefaultImageFormat
        self.system = system
        self.sysconf = conf.Platforms[self.system]
        self.tag = tag
        self.id = id
        self.meta = None
        self.import_image = True
        self.metafile = None
        self.expandedpath = None
        self.imagefile = None
        self.filepath = filepath
        self.session = session
        self.user = session.user
        self.userACL = None
        self.groupACL = None
        self.updater = Updater(id, self._null_func)

    def run(self):
        """
        Task to do the full workflow of copying an image and processing it
        """
        tag = self.tag
        logging.debug(f"img_import system={self.system} tag={tag}")
        try:
            # Step 0 - Check if path is valid
            if not transfer.check_file(self.filepath,
                                       self.sysconf,
                                       import_image=self.import_image):
                if self.import_image:
                    logging.warning(f"Import path missing {self.import_image}")
                else:
                    logging.warningf("Invalid Path: {self.filepath} {self.syscon.imageDirf}")
                raise OSError('Path not valid')
            # Step 1 - Calculate the hash of the file
            logging.debug("Starting import hashing")
            self.updater.update_status('HASHING', 'HASHING')
            self.id = transfer.hash_file(self.filepath,
                                         self.sysconf)
            # Step 2 - Populate the metadata file
            logging.debug("starting writing metadata")
            self.meta = {
                'format': self.fmt,
                'user': self.user
            }
            if not self._write_metadata():
                logging.warning("Writing metadata failed")
                raise OSError('Metadata creation failed')
            # Step 3 - Copy image and meta file from user space to shifter area
            logging.debug("starting transfer")
            imgfile = f'{self.id}.{self.fmt}'
            self.imagefile = imgfile
            self.meta['id'] = self.id
            self.updater.update_status('TRANSFER', 'TRANSFER')
            self._transfer_image()
            # if not self._transfer_image():
            #     logging.warning("Worker: Import copy failed")
            #     raise OSError("Import copy failed")

            # Done
            self.updater.update_status('READY', 'Image ready',
                                       response=self.meta)
            self._cleanup_temporary()
            return self.meta

        except Exception as e:
            err_msg = f"img_import failed sys={self.system} tag={self.tag}"
            logging.error(err_msg)
            logging.error(f"{str(e)}")
            self.updater.update_status('FAILURE', 'FAILED')

            # TODO: add a debugging flag and only disable cleanup if debugging
            self._cleanup_temporary()
            raise

    def _transfer_image(self):
        """
        Transfers the image to the target system based on the configuration.

        Returns True on success
        """
        return transfer.transfer(self.sysconf,
                                 self.filepath,
                                 self.metafile,
                                 self.import_image,
                                 self.imagefile)


class ExpireRequest(AsyncRequest):
    def __init__(self,
                 conf: Config,
                 system: str,
                 tag: str,
                 id: str,
                 ident: str):

        self.conf = conf
        self.fmt = conf.DefaultImageFormat
        self.system = system
        self.sysconf = conf.Platforms[self.system]
        self.tag = tag
        self.id = id
        self.metafile = None
        self.updater = Updater(ident, self._null_func)

    def run(self):
        """
        Remove the image to the target system based on the configuration.

        Returns True on success
        """
        logging.debug(f"do expire system={self.system} tag={self.tag}")
        self.updater.update_status('EXPIRING', 'EXPIRING')

        imagefile = f'{self.id}.{self.fmt}'
        meta = f'{self.id}.meta'
        if self.metafile:
            meta = self.metafile
        transfer.remove(self.sysconf, imagefile, meta)
        self.updater.update_status('EXPIRED', 'EXPIRED')
