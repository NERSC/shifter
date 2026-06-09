import logging
import json
import os
from shifter_imagegw import CONFIG_PATH


class Location():
    # Used to override the location as the URL
    # This is not typically required.
    url: str = None
    # Note this is deprecated but left for reference
    remotetype: str = 'dockerv2'

    # Note this is deprecated but left for reference
    authentication: str = 'http'

    def __init__(self, data: dict):
        rtype = data.get('remotetype', 'dockerv2')
        if rtype != 'dockerv2':
            raise NotImplementedError(f'Unsupported remote type {rtype}')
        self.authentication = data['authentication']
        self.url = data.get('url')


class Platform():
    accesstype = 'local'
    mungeSocketPath: str | None = None
    admins = ['root']
    imageDir = '/tmp'
    policy_file: str | None = None

    def __init__(self, data):
        if data.get('accesstype') != 'local':
            return None
        self.mungeSocketPath = data.get('mungeSocketPath')
        self.accesstype = "local"
        self.admins = data.get('admins', ['root'])
        self.imageDir = data['local'].get('imageDir', "/tmp")
        self.policy_file = data.get('policy_file')


class Config():
    WorkerThreads = 2
    LogLevel = "info"
    DefaultImageLocation = "index.docker.io"
    DefaultImageFormat = "squashfs"
    PullUpdateTimeout = 300
    ImageExpirationTimeout = "90:00:00:00"
    CacheDirectory = "/tmp/imagegw/"
    ExpandDirectory = "/tmp/imagegw/"
    MongoDBURI = None
    MongoDB = "Shifter"
    Metrics = True
    Authentication = "munge"
    ImportUsers = None
    Locations = {}
    Platforms = {}
    examiner = None
    ConverterOptions: str | list | None = None

    def __init__(self, data=None):
        if data:
            config = data
        else:
            if 'GWCONFIG' in os.environ:
                CONFIG_FILE = os.environ['GWCONFIG']
            else:
                CONFIG_FILE = f'{CONFIG_PATH}/imagemanager.json'
            # Configure logging
            logging.info(f"initializing with {CONFIG_FILE}")
            with open(CONFIG_FILE) as config_file:
                config = json.load(config_file)
        for attr in dir(self):
            if attr[0] == "_":
                continue
            if attr in config:
                val = config[attr]
                if attr in ['WorkerThreads', 'PullUpdateTimeout']:
                    val = int(config[attr])
                setattr(self, attr, val)

        for location in config['Locations']:
            data = config['Locations'][location]
            self.Locations[location] = Location(data)

        for platform in config['Platforms']:
            data = config['Platforms'][platform]
            self.Platforms[platform] = Platform(data)

        loglevel = config.get('LogLevel', 'info').lower()
        loglevel_map = {
            'debug': logging.DEBUG,
            'info': logging.INFO,
            'warn': logging.WARNING,
            'error': logging.ERROR,
            'critical': logging.CRITICAL
        }
        self.LogLevel = loglevel_map[loglevel]
        self.worker_threads = config.get('WorkerThreads')
        (days, hours, minutes, secs) = \
            list(map(lambda x: int(x), self.ImageExpirationTimeout.split(":")))

        self.expire_secs = secs + 60 * (minutes + 60 * (hours + 24 * days))
