Image Gateway Deployment Options
================================

The Image Gateway is responsible for importing images from a registry or other
sources and translating those into a format that can be used by the Shifter
Runtime layer.  The Image Gateway supports several deployment models.  The
Image Gateway can run in a local or remote mode.  In addition, the worker
component which is handles the actual conversion can be run in a distributed
mode.  We will briefly describe some of the options.  These options can be
mixed or matched when a single image gateway is being used to support multiple
systems.

Local Mode with Local Worker
----------------------------

This is the simplest deployment method and works best when the image gateway can
run directly on the target system.  The image gateway should be run on a service
node, login node, or other system that isn't used to run compute jobs. It must
have write access to a file system that is visible to all nodes on the compute
cluster.  In addition, the worker should be started on the same node.  It is
also recommended that redis (which is used between the gateway and the workers)
be run on the same node listening only on localhost to reduce potential security
risk.  Here is a sample configuration.  Notice that Mongo and Redis would need
to run on the same service node along with the celery worker.

    {
        "DefaultImageLocation": "registry-1.docker.io",
        "DefaultImageFormat": "squashfs",
        "PullUpdateTimeout": 300,
        "ImageExpirationTimeout": "90:00:00:00",
        "MongoDBURI":"mongodb://localhost/",
        "MongoDB":"Shifter",
        "Broker":"redis://localhost/",
        "CacheDirectory": "/images/cache/",
        "ExpandDirectory": "/images/expand/",
        "Locations": {
            "registry-1.docker.io": {
                "remotetype": "dockerv2",
                "authentication": "http"
            }
        },
        "Platforms": {
            "mycluster": {
                "mungeSocketPath": "/var/run/munge/munge.socket.2",
                "accesstype": "local",
                "admins": ["root"],
                "local": {
                    "imageDir": "/images"
                }
            }
        }
    }

Remote Mode with a Local Worker
-------------------------------
In this model, the Image Gateway and worker runs on a system external to the
target system.  After an image has been converted, it is copied using scp to the
target system. This model may be used when multiple systems are being supported and their are
concerns with the security model of Redis.  This model may also be used if the
entire compute system is inside a firewall since the Image Gateway could run
on a system entirely outside the cluster.  The deployment must be configured
with ssh keys between that can be used between the Image Gateway and a login
or service node on the target system.  We recommend that the Image Gateway and
worker(s) be run as a dedicated non-privileged user (e.g. shifter).  A similar
account should exist on the target system and an RSA-based key-pair should be
configured on the target system with a copy of the private key available to
the special account on the Image Gateway.  Here is a sample configuration for
this deployment model.

    {
        "DefaultImageLocation": "registry-1.docker.io",
        "DefaultImageFormat": "squashfs",
        "PullUpdateTimeout": 300,
        "ImageExpirationTimeout": "90:00:00:00",
        "MongoDBURI":"mongodb://localhost/",
        "MongoDB":"Shifter",
        "Broker":"redis://localhost/",
        "CacheDirectory": "/images/cache/",
        "ExpandDirectory": "/images/expand/",
        "Locations": {
            "registry-1.docker.io": {
                "remotetype": "dockerv2",
                "authentication": "http"
            }
        },
        "Platforms": {
            "mycluster": {
                "mungeSocketPath": "/var/run/munge/munge.socket.2",
                "accesstype": "remote",
                "admins": ["root"],
                "host": [
                    "mycluster01"
                ],
                "ssh": {
                    "username": "shifter",
                    "key": "/home/shifter/.ssh/ssh.key",
                    "imageDir": "/images"
                }
            }
        }
    }

Local Mode with a Remote Worker
-------------------------------
In this model, the Image Gateway runs on a different host from the worker which
runs on a service node on the target cluster.  This mode works best for scenarios
where a single gateway is being used to support multiple clusters.  The
workers are run locally on the cluster for performance reasons since then the
prepared image can be directly created on the target system without an extra
copy operation which can be slow for very large images.  The primary drawback
to this approach is the Redis server used to communicate between the gateway
and the worker must be secured.  We recommend that this approach only be used
if you can restrict connections to the redis server to only allow connections
from the gateway and workers.  The only difference in the configuration is the
redis server is remote.  In this example redis is running on a host called
"redis-server"

    {
        "DefaultImageLocation": "registry-1.docker.io",
        "DefaultImageFormat": "squashfs",
        "PullUpdateTimeout": 300,
        "ImageExpirationTimeout": "90:00:00:00",
        "MongoDBURI":"mongodb://localhost/",
        "MongoDB":"Shifter",
        "Broker":"redis://localhost/",
        "CacheDirectory": "/images/cache/",
        "ExpandDirectory": "/images/expand/",
        "Locations": {
            "registry-1.docker.io": {
                "remotetype": "dockerv2",
                "authentication": "http"
            }
        },
        "Platforms": {
            "mycluster": {
                "mungeSocketPath": "/var/run/munge/munge.socket.2",
                "accesstype": "remote",
                "admins": ["root"],
                "host": [
                    "mycluster01"
                ],
                "ssh": {
                    "username": "shifter",
                    "key": "/home/shifter/.ssh/ssh.key",
                    "imageDir": "/images"
                }
            }
        }
    }
