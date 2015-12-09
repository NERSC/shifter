# Image Gateway API

The image gateway handles creating, tracking, and cleaning up images.  It provides a RESTful interface for client interactions, a manager layer that manages and tracks the images, and workers that do most of the fetching, packing and transferring of images.

## API

The image manager provides a RESTful API.  It has three main verbs that it supports.  The API is a thin translation layer on top of the manager layer.  Most of the actual action is handled by functions in the management layer.

### Lookup

TODO

### Pull

TODO

### List

TODO

### Expire

TODO

## Manager layer

The manager layer contains functions that map to the API layer but also has a
number of helper functions.  The manager layer uses a Mongo database to keep
track of the images.  It uses Celery to asynchronously spawn task to pull, pack,
and transfer images.

### Configuration

TODO

### Troubleshooting

TODO

## workers

The image manager uses Celery based workers to do the low-level image
manipulation.  This separation is primarily so the operations can be queued to
prevent overloading the image manager, but also enables the workers being
remotely executed.

### Configuration

TODO

### Troubleshooting

TODO
