# Image Gateway API

The image gateway handles creating, tracking, and cleaning up images.  It provides a RESTful interface for client interactions, a manager layer that manages and tracks the images, and workers that do most of the fetching, packing and transferring of images.

To start the image gateway do something like:

    ## May need to add something to PYTHONPATH depending on where shifter_imagegw
    ## is installed
    gunicorn -b 0.0.0.0:5000 --backlog 2048 \
        --access-logfile=/var/log/shifter_imagegw/access.log \
        --log-file=/var/log/shifter_imagegw/error.log \
        shifter_imagegw.api:app

## Start Gateway with Docker and Docker-Compose

If docker and docker-compose are installed, you can try starting a test environment with docker-compose.  There is a Makefile
target that makes this easy.

    make starttest

This will create a munge key and ssh keys in test/config.  This directory gets volumed mounted into the API and Worker images.
See the docker-compose.yml for details.  You can modify the docker-compose and create a configuration directory to run a non-test
instance of the API and worker service.

## API

The image manager provides a RESTful API.  It has three main verbs that it supports.  The API is a thin translation layer on top of the manager layer.  Most of the actual action is handled by functions in the management layer.

### Lookup

curl -H "authentication: mungehash" -X GET http://localhost:5555/api/lookup/system/docker/ubuntu:latest

### Pull

curl -H "authentication: mungehash" -X POST http://localhost:5555/api/pull/system/docker/ubuntu:latest

### List

TODO

### Expire

Not fully implemented yet.

## Manager layer

The manager layer contains functions that map to the API layer but also has a
number of helper functions.  The manager layer uses a Mongo database to keep
track of the images.  It uses Celery to asynchronously spawn task to pull, pack,
and transfer images.

### Configuration

See imagemanger.json

### Troubleshooting

TODO

## workers

The image manager uses Celery based workers to do the low-level image
manipulation.  This separation is primarily so the operations can be queued to
prevent overloading the image manager, but also enables the workers being
remotely executed.

### Configuration

See imagemanager.json

### Troubleshooting

TODO
