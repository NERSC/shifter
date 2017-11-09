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

import json
import os
import shutil
import sys
import subprocess
import logging
import tempfile
from multiprocessing.queues import Queue
from multiprocessing.pool import ThreadPool
from time import time, sleep
from random import randint
from shifter_imagegw import CONFIG_PATH, dockerv2, converters, transfer

CONFIG = None

if 'GWCONFIG' in os.environ:
    CONFIGFILE = os.environ['GWCONFIG']
else:
    CONFIGFILE = '%s/imagemanager.json' % (CONFIG_PATH)

logging.info("Opening %s", CONFIGFILE)

with open(CONFIGFILE) as configfile:
    CONFIG = json.load(configfile)

if 'CacheDirectory' in CONFIG:
    if not os.path.exists(CONFIG['CacheDirectory']):
        os.mkdir(CONFIG['CacheDirectory'])
if 'ExpandDirectory' in CONFIG:
    if not os.path.exists(CONFIG['ExpandDirectory']):
        os.mkdir(CONFIG['ExpandDirectory'])


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


class WorkerThreads(object):
    def __init__(self, threads=1):
        """
        Initialize the thread pool and queues.
        """
        self.pools = ThreadPool(processes=threads)
        self.updater_queue = Queue()

    def get_updater_queue(self):
        return self.updater_queue

    def updater(self, ident, state, meta):
        """
        Updater function: This just post a message to a queue.
        """
        self.updater_queue.put({'id': ident, 'state': state, 'meta': meta})

    def pull(self, request, updater, testmode=0):
        try:
            pull(request, updater, testmode=testmode)
        except Exception as err:
            resp = {'error_type': str(type(err)),
                    'message': str(err)}
            updater.update_status('FAILURE', 'FAILURE', response=resp)

    def expire(self, request, updater):
        try:
            remove_image(request, updater)
        except Exception as err:
            resp = {'error_type': str(type(err)),
                    'message': str(err)}
            updater.update_status('FAILURE', 'FAILURE', response=resp)

    def dopull(self, ident, request, testmode=0):
        """
        Kick off a pull operation.
        """
        updater = Updater(ident, self.updater)
        self.pools.apply_async(self.pull, [request, updater],
                               {'testmode': testmode})

    def doexpire(self, ident, request, testmode=0):
        updater = Updater(ident, self.updater)
        self.pools.apply_async(self.expire, [request, updater])


def _get_cacert(location):
    """ Private method to get the cert location """
    params = CONFIG['Locations'][location]
    cacert = None
    currdir = os.getcwd()
    if 'sslcacert' in params:
        if params['sslcacert'].startswith('/'):
            cacert = params['sslcacert']
        else:
            cacert = '%s/%s' % (currdir, params['sslcacert'])
        if not os.path.exists(cacert):
            raise OSError('%s does not exist' % cacert)
    return cacert


def _pull_dockerv2(request, location, repo, tag, updater):
    """ Private method to pull a docker images. """
    cdir = CONFIG['CacheDirectory']
    edir = CONFIG['ExpandDirectory']
    params = CONFIG['Locations'][location]
    cacert = _get_cacert(location)

    url = 'https://%s' % location
    if 'url' in params:
        url = params['url']
    try:
        options = {}
        if cacert is not None:
            options['cacert'] = cacert
        options['baseUrl'] = url
        if 'authMethod' in params:
            options['authMethod'] = params['authMethod']

        if ('session' in request and 'tokens' in request['session'] and
                request['session']['tokens']):
            if location in request['session']['tokens']:
                userpass = request['session']['tokens'][location]
                options['username'] = userpass.split(':')[0]
                options['password'] = ''.join(userpass.split(':')[1:])
            elif ('default' in request['session']['tokens']):
                userpass = request['session']['tokens']['default']
                options['username'] = userpass.split(':')[0]
                options['password'] = ''.join(userpass.split(':')[1:])
        imageident = '%s:%s' % (repo, tag)
        dock = dockerv2.DockerV2Handle(imageident, options, updater=updater)
        updater.update_status("PULLING", 'Getting manifest')
        manifest = dock.get_image_manifest()
        request['meta'] = dock.examine_manifest(manifest)
        request['id'] = str(request['meta']['id'])

        if check_image(request):
            return True

        dock.pull_layers(manifest, cdir)

        expandedpath = tempfile.mkdtemp(suffix='extract',
                                        prefix=request['id'], dir=edir)
        request['expandedpath'] = expandedpath

        updater.update_status("PULLING", 'Extracting Layers')
        dock.extract_docker_layers(expandedpath, dock.get_eldest_layer(),
                                   cachedir=cdir)
        return True
    except:
        logging.warn(sys.exc_value)
        raise

    return False


def pull_image(request, updater):
    """
    pull the image down and extract the contents

    Returns True on success
    """
    params = None
    rtype = None

    # See if there is a location specified
    location = CONFIG['DefaultImageLocation']
    tag = request['tag']
    if tag.find('/') > 0:
        parts = tag.split('/')
        if parts[0] in CONFIG['Locations']:
            # This is a location
            location = parts[0]
            tag = '/'.join(parts[1:])

    parts = tag.split(':')
    if len(parts) == 2:
        (repo, tag) = parts
    else:
        raise OSError('Unable to parse tag %s' % request['tag'])
    logging.debug("doing image pull for loc=%s repo=%s tag=%s", location,
                  repo, tag)

    if location in CONFIG['Locations']:
        params = CONFIG['Locations'][location]
        rtype = params['remotetype']
    else:
        raise KeyError('%s not found in configuration' % location)

    if rtype == 'dockerv2':
        return _pull_dockerv2(request, location, repo, tag, updater)
    elif rtype == 'dockerhub':
        logging.warning("Use of depcreated dockerhub type")
        raise NotImplementedError('dockerhub type is depcreated. Use dockerv2')
    else:
        raise NotImplementedError('Unsupported remote type %s' % rtype)
    return False


def examine_image(request):
    """
    examine the image

    Returns True on success
    """

    if 'examiner' in CONFIG:
        examiner = CONFIG['examiner']
        retcode = subprocess.call([examiner, request['expandedpath'],
                                   request['id']])
        if retcode != 0:
            return False

    return True


def get_image_format(request):
    """
    Retreive the image format for the reuqest using a default if not provided.
    """
    fmt = CONFIG['DefaultImageFormat']
    if fmt in request:
        fmt = request['format']

    return fmt


def convert_image(request):
    """
    Convert the image to the required format for the target system

    Returns True on success
    """
    fmt = get_image_format(request)
    request['format'] = fmt

    edir = CONFIG['ExpandDirectory']

    imagefile = os.path.join(edir, '%s.%s' % (request['id'], fmt))
    request['imagefile'] = imagefile

    status = converters.convert(fmt, request['expandedpath'], imagefile)
    return status


def write_metadata(request):
    """
    Write out the metadata file

    Returns True on success
    """
    fmt = request['format']
    meta = request['meta']
    if 'userACL' in request:
        meta['userACL'] = request['userACL']
    if 'groupACL' in request:
        meta['groupACL'] = request['groupACL']

    edir = CONFIG['ExpandDirectory']

    # initially write metadata to tempfile
    (fdesc, metafile) = tempfile.mkstemp(prefix=request['id'], suffix='meta',
                                         dir=edir)
    os.close(fdesc)
    request['metafile'] = metafile

    status = converters.writemeta(fmt, meta, metafile)

    # after success move to final name
    final_metafile = os.path.join(edir, '%s.meta' % (request['id']))
    shutil.move(metafile, final_metafile)
    request['metafile'] = final_metafile

    return status


def check_image(request):
    """
    Checks if the target image is on the target system

    Returns True on success
    """
    system = request['system']
    if system not in CONFIG['Platforms']:
        raise KeyError('%s is not in the configuration' % system)
    sysconf = CONFIG['Platforms'][system]

    fmt = get_image_format(request)
    image_filename = "%s.%s" % (request['id'], fmt)
    image_metadata = "%s.meta" % (request['id'])

    return transfer.imagevalid(sysconf, image_filename, image_metadata,
                               logging)


def transfer_image(request, meta_only=False):
    """
    Transfers the image to the target system based on the configuration.

    Returns True on success
    """
    system = request['system']
    if system not in CONFIG['Platforms']:
        raise KeyError('%s is not in the configuration' % system)
    sysconf = CONFIG['Platforms'][system]
    meta = None
    if 'metafile' in request:
        meta = request['metafile']
    if meta_only:
        request['meta']['meta_only'] = True
        return transfer.transfer(sysconf, None, meta, logging)
    else:
        return transfer.transfer(sysconf, request['imagefile'], meta, logging)


def remove_image(request, updater):
    """
    Remove the image to the target system based on the configuration.

    Returns True on success
    """
    logging.debug("do expire system=%s tag=%s", request['system'],
                  request['tag'])
    updater.update_status('EXPIRING', 'EXPIRING')

    system = request['system']
    if system not in CONFIG['Platforms']:
        raise KeyError('%s is not in the configuration' % system)
    sysconf = CONFIG['Platforms'][system]
    imagefile = request['id'] + '.' + request['format']
    meta = request['id'] + '.meta'
    if 'metafile' in request:
        meta = request['metafile']
    if transfer.remove(sysconf, imagefile, meta, logging):
        updater.update_status('EXPIRED', 'EXPIRED')
    else:
        logging.warn("Worker: Expire failed")
        raise OSError('Expire failed')


def cleanup_temporary(request):
    """
    Helper function to cleanup any temporary files or directories.
    """
    items = ('expandedpath', 'imagefile', 'metafile')
    for item in items:
        if item not in request or request[item] is None:
            continue
        cleanitem = request[item]
        if isinstance(cleanitem, unicode):
            cleanitem = str(cleanitem)

        if not isinstance(cleanitem, str):
            raise ValueError('Invalid type for %s,%s' %
                             (item, type(cleanitem)))
        if cleanitem == '' or cleanitem == '/':
            raise ValueError('Invalid value for %s: %s' % (item, cleanitem))
        if not cleanitem.startswith(CONFIG['ExpandDirectory']):
            raise ValueError('Invalid location for %s: %s' % (item, cleanitem))
        if os.path.exists(cleanitem):
            logging.info("Worker: removing %s", cleanitem)
            try:
                subprocess.call(['chmod', '-R', 'u+w', cleanitem])
                if os.path.isdir(cleanitem):
                    shutil.rmtree(cleanitem, ignore_errors=True)
                else:
                    os.unlink(cleanitem)
            except:
                logging.error("Worker: caught exception while trying to "
                              "clean up (%s) %s.", item, cleanitem)


def pull(request, updater, testmode=0):
    """
    Main task to do the full workflow of pulling an image and transferring it
    """
    tag = request['tag']
    logging.debug("dopull system=%s tag=%s", request['system'], tag)
    if testmode == 1:
        states = ('PULLING', 'EXAMINATION', 'CONVERSION', 'TRANSFER')
        for state in states:
            logging.info("Worker: testmode Updating to %s", state)
            updater.update_status(state, state)
            sleep(1)
        ident = '%x' % randint(0, 100000)
        ret = {
            'id': ident,
            'entrypoint': ['./blah'],
            'workdir': '/root',
            'env': ['FOO=bar', 'BAZ=boz']
        }
        state = 'READY'
        updater.update_status(state, state, ret)
        return ret
    elif testmode == 2:
        logging.info("Worker: testmode 2 setting failure")
        raise OSError('task failed')
    try:
        # Step 1 - Do the pull
        updater.update_status('PULLING', 'PULLING')
        logging.info(request)
        if not pull_image(request, updater=updater):
            logging.info("Worker: Pull failed")
            raise OSError('Pull failed')

        if 'meta' not in request:
            raise OSError('Metadata not populated')

        if not check_image(request):
            # Step 2 - Check the image
            updater.update_status('EXAMINATION', 'Examining image')
            logging.debug("Worker: examining image %s" % tag)
            if not examine_image(request):
                raise OSError('Examine failed')
            # Step 3 - Convert
            updater.update_status('CONVERSION', 'Converting image')
            logging.debug("Worker: converting image %s" % tag)
            if not convert_image(request):
                raise OSError('Conversion failed')
            if not write_metadata(request):
                raise OSError('Metadata creation failed')
            # Step 4 - TRANSFER
            updater.update_status('TRANSFER', 'Transferring image')
            logging.debug("Worker: transferring image %s", tag)
            if not transfer_image(request):
                raise OSError('Transfer failed')
        else:
            logging.debug("Need to update metadata")
            request['format'] = get_image_format(request)

            if not write_metadata(request):
                raise OSError('Metadata creation failed')
            updater.update_status('TRANSFER', 'Transferring metadata')
            logging.debug("Worker: transferring metadata %s", request['tag'])
            if not transfer_image(request, meta_only=True):
                raise OSError('Transfer failed')

        # Done
        updater.update_status('READY', 'Image ready', response=request['meta'])
        cleanup_temporary(request)
        return request['meta']

    except:
        logging.error("ERROR: dopull failed system=%s tag=%s",
                      request['system'], request['tag'])
        print sys.exc_value
        updater.update_status('FAILURE', 'FAILED')

        # TODO: add a debugging flag and only disable cleanup if debugging
        cleanup_temporary(request)
        raise
