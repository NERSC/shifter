.. _shifter-modules:

Shifter Modules
===============

To adapt containers to a particular system and meet specific user needs,
Shifter provides sites the ability to configure "modules" which more flexibly
implement the the globally applied modifications that Shifter can perform
for all container invocations.

A Shifter module allows specific content to be injected into the container,
environment variables to be manipulated, and hooks both as root during
container instantiation (not _within_ the container), and as the user
(in the container) immediately before process start.  The capabilities
allow a site to customize all user containers and provide local support for
a variety of system capabilities, and then give the users the option to use
the local support (enable the module) or disable it if their container has
other needs.

Example Modules:
================

CVMFS at NERSC:
---------------
NERSC makes CVMFS available on the /cvmfs_nfs mount point, where cvmfs is
made available via NFS and aggressively cached in a number of ways.

If a user wants to use it in their container, a simple module could be:

udiRoot.conf:
module_cvmfs_siteFs = /cvmfs_nfs:/cvmfs
module_cvmfs_siteEnv = SHIFTER_MODULE_CVMFS=1

Then the shifter invocation would be:

shifter --image=<image> --module=cvmfs ...

This can also be achieved with volume mounts, but the module system allows
a user to _avoid_ getting cvmfs, unless they want it.

Cray mpich Support
==================

Cray mpich support makes use of the ABI compatibility interface.  If a user has
an application linked against vanilla mpich, then the Cray version can be
interposed via linking operations.

Thus if the site prepares a copy of the CrayPE environment for use by shifter
(using the prep_cray_mpi_libs.py script in the "extra" directory of the shifter
distribution), then uses the copyPath (or siteFs) mechanism to inject those
libraries into the container, and finally uses LD_LIBRARY_PATH to force the
application to see the new version, then the user can use the same container on
a variety of systems.

udiRoot.conf:
module_mpich_copyPath = /shared/shifter/modules/mpich
module_mpich_siteEnvPrepend = LD_LIBRARY_PATH=/opt/udiImage/modules/mpich/lib64
module_mpich_userhook = /opt/udiImage/modules/mpich/bin/init.sh

If the user prefers their own mpich version, and wants to make use of the
shifter ssh interface, then they can avoid using the mpich module.  If the site
makes the mpich module default, the user can still disable it with by specifying
"--module=none", which turns off all modules.

The userhook script above is executed immediately prior to the user process 
being executed and can be used to validate that the application in the
container is compatible with the MPI libraries being injected.

