Updating Shifter
================

Version 16.08 to 17.04
----------------------
Configurations should be backwards compatible.  There are new optional parameters
that have been added.

**udiRoot.conf**

   * New optional parameter (siteResources).  This has the location of libraries
     that should automatically be bind mounted into the container.

**imagemanager.json**

   * New optional parameter (Metrics). True/False parameter to enable basic
     image metrics.

Version 15.12.0 to 16.08.1
--------------------------

**udiRoot.conf**

   * siteFs format changed from space separated to semicolon separated.  Now
     requires that both "to" and "from" locations be specified.  Recommend
     using same in most cases::

        siteFs=/home:/home;\
               /var/opt/cray/spool:/var/opt/cray/spool:rec

     siteFs now also supports recursive bind mounts (as well as a few other
     less common mount options).

   * Recommended Setting: defaultImageType=docker
     This will allow shifter commands (shifter, shifterimg, SLURM integration)
     to assume, unless otherwise noted, that images requested are docker
     images.  This enables the user to do::

         shifter --image=ubuntu:14.04

     Instead of::

         shifter --image=docker:ubuntu:14.04

**sshd**

The sshd is no longer started by default by the SLURM integration.  Add
"enable_sshd=1" to plugstack.conf if you want it.

CCM mode is no longer enabled by default in the SLURM integration. Add
"enable_ccm=1" to plugstack.conf if you want it.

If the sshd is configured to run via the WLM, the sshd now runs as the user by
default.  If you have an existing udiImage directory that you want to keep
using, be sure to update the port in sshd_config and ssh_config to use port
1204 (or some other port that makes sense for your site).  To have setupRoot
start the sshd as root set "optionalSshdAsRoot=1" in udiRoot.conf

Image Manager API
=================
We recommend that you install and run the image manager via gunicorn.  This is
much more scalable than the Flask development server started by the older (now
considered for debug-only) imagemngr.py.

To start with gunicorn do::

    /usr/bin/gunicorn -b 0.0.0.0:5000 --backlog 2048 \
        --access-logfile=/var/log/shifter_imagegw/access.log \
        --log-file=/var/log/shifter_imagegw/error.log \
        shifter_imagegw.api:app

**Image Manager Workers**

The workers have been updated to do a better job of efficiently converting
Docker images into Shifter's preferred squashfs format.  However, Celery (the
vehicle by which workers are started) requires a minor configuration change to
properly support unicode filenames in the converted images.  Start Celery with
the included sitecustomize.py in your PYTHONPATH.  This is typically installed
in the libexec directory (in Redhat), or the lib directory (in SuSE).

e.g.,::

    PYTHONPATH=/usr/libexec/shifter /usr/bin/celery \
               -A shifter_imagegw.imageworker \
               worker \
               -Q $MACHINE \
               -n $MACHINE.%h \
               --loglevel=debug \
               --logfile=/var/log/shifter_imagegw_worker/$MACHINE.log

where $MACHINE above refers to your cluster name.
