Installing Shifter on a RHEL/Centos/Scientific Linux 6 System
*************************************************************

Building RPMs
=============

First, ensure your build system has all necessary packages installed::

    yum install epel-release
    yum install rpm-build gcc glibc-devel munge libcurl-devel json-c \
        json-c-devel pam-devel munge-devel libtool autoconf automake \
        gcc-c++ python-pip xfsprogs squashfs-tools python-devel

Next, if not using a prepared source tarball, generate one from the repo::

    git clone https://github.com/NERSC/shifter.git
    [[ perform any needed git operations to get a particular branch or commit
       you require ]]
    VERSION=$(grep Version: shifter/shifter.spec | awk '{print $2}')
    cp -rp shifter "shifter-$VERSION"
    tar cf "shifter-$VERSION.tar.gz" "shifter-$VERSION"

Build the RPMs from your tarball::

    rpmbuild -tb "shifter-$VERSION.tar.gz"

*Note about Slurm support*
To build with Slurm support do::

    rpmbuild -tb "shifter-$VERSION.tar.gz" --define "with_slurm /usr"

Change "/usr" to the base path Slurm is installed in.

Installing the Image Manager
============================

The image manager system can run on a login node or other service node in your
cluster so long as it is running munge using the same munge key as all the
compute nodes (and login nodes) and all nodes can access the imagegwapi port
(typically 5000) on the image manager system.

Install the needed dependencies and shifter RPMs::

    yum install epel-release
    yum install python python-pip munge json-c squashfs-tools
    rpm -i /path/to/rpmbuild/RPMS/x86_64/shifter-imagegw-$VERSION.rpm
    ## shifter-runtime is optional, but recommended on the image gateway system
    rpm -i /path/to/rpmbuild/RPMS/x86_64/shifter-runtime-$VERSION.rpm


Configuring the Image Manager
=============================
Copy /etc/shifter/imagemanager.json.example to /etc/shifter/imagemanager.json.
At minimum you should check that:

* "MongoDBURI" is correct URL to shifter imagegw mongodb server
* "CacheDirectory" exists, semi-permanent storage for docker layers
* "ExpandDirectory" exists, temporary storage for converting images
* Change "mycluster" under "Platforms" to match your system name, should match the "system" configuration in udiRoot.conf
* Ensure the "imageDir" is set correctly for your system

The imageDir should be a network volume accessible on all nodes in the system.
The CacheDirectory and ExpandDirectory need only be visible on the imagegw
system.

See TODO:FutureDocument for more information on imagegw configuration.

Installing the Shifter Runtime
==============================

The shifter runtime needs to be installed on the login nodes as well as the 
compute nodes.

Install the needed dependencies and shifter RPMs::

    yum install epel-release
    yum install json-c munge

    rpm -i /path/to/rpmbuild/RPMS/x86_64/shifter-runtime-$VERSION.rpm

Configuring the Shifter Runtime
===============================
Copy /etc/shifter/udiRoot.conf.example to /etc/shifter/udiRoot.conf
At minimum you need to change:

* set the value for "system" to match the platform name from
  imagemanager.json
* set the URL for imageGateway to match your imagegw machine, no trailing slash

Generate a passwd and group file for all your shifter users and place in:

* ``/etc/shifter/etc_files/passwd``
* ``/etc/shifter/etc_files/group``

Often, these can be generated as easily as
``getent passwd > /etc/shifter/etc_files/passwd``, however you'll need to setup
to match your local user management configuration.  The path to these
share etc files for all shifter containers can be controlled with the *etcPath*
configuration in udiRoot.conf.  It is recommended that it be on a network
volume to ease updating the passwd and group files.

See TODO:FutureDocument for more information on udiRoot.conf configuration.

Starting the Image Manager
==========================

Ensure that mongod is running, if configured to be on the same host as
the imagegw, do something like:

1. ``yum install mongodb-server``
2. ``/etc/init.d/mongod start``

TODO:  put init scripts into RPM distribution
Without init scripts, do something like:

``/usr/libexec/shifter/imagegwapi.py > /var/log/imagegwapi.log &``

   * Ensure that CLUSTERNAME matches the values in udiRoot.conf (system) and imagemanger.json (platform)
