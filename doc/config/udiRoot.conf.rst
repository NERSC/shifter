udiRoot.conf reference
**********************

udiRoot.conf is read by shifter and most other related shifter utilities within
the udiRoot component.  Unless udiRoot is built enabling particular options
udiRoot.conf must be owned by root, but readable by all users, or at least
all users you want accessing shifter.

Configuration File Format
=========================
The file configuration format is a basic key=value, however space seperated 
strings can be used for multiple options. Multiple lines can be used if the
final character on the line is '\'.  Items cannot be quoted to allow spaces
within the configuration option.

Configuration File Options
==========================

udiMount (required)
-------------------
Absolute path to where shifter should generate a mount point for its own use.
This path to this should be writable by root and should not be in use for 
other purposes.

Recommended value:  /var/udiMount

loopMount (required)
--------------------
Absolute path to where shifter should mount loop device filesystems.  This
path should be writable by root and should not be in use for other purposes.

Recommended value: /var/udiLoopMount

imagePath (required)
--------------------
Absolute path to where shifter can find images.  This path should be readable
by root.  This path should be visible by all nodes in the system.  It may be
possible to use some kind of rsyncing method to have this path be local on
all systems, but that may prove problematic if a user attempts to use an image
while it is being rsynced.  Recommend using GPFS or lustre or similar.

udiRootPath (required)
----------------------
Absolute path (can be a symlink) to where current version of udiRoot is
installed.  This path is used to find all needed shifter-specific utilities
(shifter, shifterimg, setupRoot, unsetupRoot, mount, etc).

Recommended value: /opt/shifter/default

sitePreMountHook
----------------
Script to be run before bind-mounting the siteFs filesystems.  This script
needs to be root owned and executable by root.  It should create any directories
on the path to the mount point, but not the mount point itself (e.g.,
        `mkdir -p global`
        but not 
        `mkdir -p global/u1`
        if your siteFs path is /global/u1
        )

Note that the script is executed within your udiMount directory and so all your
paths within the script should be relative to that.

Recommended value: /etc/opt/nersc/udiRoot/premount.sh

sitePostMountHook
-----------------
Script to be run after bind-mounting the siteFs filesystems.  This script
need to be root owned and executable by root.  It should do any work required
after performing the mounts, e.g., generating a symlink.

Note that the script is executed within your udiMount directory and so all
your paths within the script should be relative to that.

Recommended value: /etc/opt/nersc/udiRoot/postmount.sh

optUdiImage
-----------
Absolute path to the udiImage directory to be bind-mounted onto /opt/udiImage.
This is typically pre-built with shifter to include an sshd, but you could
add other things if you so desire.

Recommended value: /opt/shifter/udiRoot/default/deps/udiImage

etcPath
-------
Absolute path to the files you want copied into /etc for every container.
This path must be root owned (including the files within), and it must
contain, at minimum, nsswitch.conf, passwd, group.

Note that any files you put in this path will override whatever the user
included in their image.

Recommended value: /opt/shifter/default/etc_files

allowLocalChroot (0 or 1)
-------------------------
shifter can be used to construct a "container" out a of local path instead
of a loop device filesystem.  This can be useful if you have an unpacked
layer you want to examine, or to enable shifter services within an existing
path.  Setting to 1 will allow this path-specified shifting, 0 will not.

This must be enabled if the "ccm" emulation mode is desired.  (ccm emulation
is effectively done with `shifter --image=local:/` within the Slurm integration.

autoLoadKernelModule (0 or 1)
-----------------------------
Flag to determine if kernel modules can be automatically loaded by shifter if
required.  This is typically limited to loop, squashfs, ext4 (and its
dependencies)

Recommend 0 if you already load loop, squashfs, and ext4 as part of node bootup
process.

Recommend 1 if you want to let shifter load them for you.

mountUdiRootWritable (required)
-------------------------------
Flag to remount the udiMount VFS read-only after setup.  This is typically only
needed for debugging, and should usually be set to 1.

Recommended value: 1

maxGroupCount (required)
------------------------
Maximum number of groups to allow.  If the embedded sshd is being used, then
this should be set to 31.  This is used when preparing the /etc/group file, which
is a filtered version of the group file you provide to shifter.  The filtering is
done because the libc utilities for parsing an /etc/group file are typically more
limited than the LDAP counterparts.  Since LDAP is not usable within shifter, a
filtered group file is used.

Recommended value: 31

modprobePath (required)
-----------------------
Absolute path to known-good modprobe

insmodPath (required)
---------------------
Absolute path to known-good insmod

cpPath (required)
-----------------
Absolute path to known-good cp

mvPath (required)
-----------------
Absolute path to known-good mv

chmodPath (required)
--------------------
Absolute path to known-good chmod

mkfsXfsPath
-----------
Absolute path to known-good mkfs.xfs. This is required for the perNodeCache
feature to work.

rootfsType (required)
---------------------
The filesystem type to use for setting up the shifter VFS layer.
This is typically just tmpfs.  On cray compute nodes (CLE 5.2 and 6.0),
tmpfs will not work, instead use ramfs. 

Recommended value: tmpfs

gatewayTimeout (optional)
-------------------------
Time in seconds to wait for the imagegw to respond before
failing over to next (or failing).

siteFs
------
Space seperated list of paths to be automatically bind-mounted into
the container.  This is typically used to make network filesystems
accessible within the container, but could be used to allow certain
other facilities, like /var/run or /var/spool/alps to be accessible
within the image (depending on your needs).

Do not attempt to bind things under /usr or other common critical
paths within containers.

It is OK to perform this under /var or /opt or a novel path that 
your site maintains (e.g., for NERSC, /global).

siteEnv
-------
Space seperated list of environment variables to automatically set (or
add, or replace) when a shifter container is setup.

Example::
    siteEnv=SHIFTER_RUNTIME=1

This can be useful if network home directories are mounted into the 
container and you users want a way to prevent their localized dotfiles
from running. (e.g., do not execute if SHIFTER_RUNTIME is set).

siteEnvAppend
-------------
Space seperated list of environment variables to automatically append (or
add) when a shifter container is setup.  This only makes sense for colon
seperated environment variables, .e.g, PATH.

Example::
    siteEnvAppend=PATH=/opt/udiImage/bin

This can be used if your site patches in a path that you want to appear in
the path.  Recommend that all binaries are compatible with all containers,
i.e., are statically linked, to ensure they work.

siteEnvPrepend
--------------
Space seperated list of environment variables to automatically prepend (or
add) when a shifter container is setup.  This only makes sense for colon
seperated environment variables, e.g., PATH.

Example::
    siteEnvPrepend=PATH=/opt/udiImage/bin

This can be used if your site patches in a path that you want to appear in
the path.  Recommend that all binaries are compatible with all containers,
i.e., are statically linked, to ensure they work.

siteEnvUnset
------------
Space separated list of environment variables to be unset when a shifter
container is setup.  This only makes sense for bare environmental variable
names.

Example::
    siteEnvUnset=LOADEDMODULES _LMFILES_

imageGateway
------------
Space seperated URLs for your imagegw.  Used by shifterimg and Slurm batch
integration to communicate with the imagegw.

batchType (optional)
--------------------
Used by batch integration code to pick useful settings.  May be deprecated
in the future as it is not necessary at this point.

system (required)
-----------------
Name of your system, e.g., edison or cori.  This name must match a configured
system in the imagegw.  This is primarily used by shifterimg to self-identify
which system it is representing.

Shifter Module Options in udiRoot.conf
======================================

For each module a site wants to configure, at least one of the `module_*`
configuration parameters needs to include.  Only specify the options that
are needed for your module.

Note that modules will be loaded in the order the user specifies.  By
specifying a custom set of modules, the user will disable whatever modules
were specified by the administrator in the defaultModules configuration
parameter.

The siteEnv* parameters are evaluated after all modules have been evaluated
when performing environmental setup.

defaultModules
--------------
comma-separated list of modules that should be loaded by default for every
container invocation.  The user can override this and provide their own list
of modules that are appropriate for their need.

module_<name>_siteEnv
---------------------
Like siteEnv, allows the site to define the value an environment variable
should take when setting up the container environment, but only if the target
module is activated.  This value will replace anything set up previously either
in the external environment, user-specified option, or in the container
definition.  The global siteEnv can override this.

module_<name>_siteEnvPrepend
----------------------------
Space seperated list of environment variables to automatically append (or
add) when a shifter container is setup.  This only makes sense for colon
seperated environment variables, .e.g, PATH.

Example::
    module_<name>_siteEnvAppend=PATH=/opt/udiImage/modules/<name>/bin

This can be used if your site patches in a path that you want to appear in
the path.  Recommend that all binaries are compatible with all containers,
i.e., are statically linked, to ensure they work.
The global siteEnvPrepend is applied after this.

module_<name>_siteEnvAppend
---------------------------
Space seperated list of environment variables to automatically append (or
add) when a shifter container is setup.  This only makes sense for colon
seperated environment variables, .e.g, PATH.

Example::
    module_<name>_siteEnvAppend=PATH=/opt/udiImage/modules/<name>/bin

This can be used if your site patches in a path that you want to appear in
the path.  Recommend that all binaries are compatible with all containers,
i.e., are statically linked, to ensure they work.
The global siteEnvAppend is applied after this.

module_<name>_siteEnvUnset
--------------------------
Space separated list of environment variables to be unset when a shifter
container is setup.  This only makes sense for bare environmental variable
names.

Example::
    module_<name>_siteEnvUnset=MPICH_...

module_<name>_conflict
----------------------
Space separated list of other modules that cannot be concurrently loaded
with this module.  Attempt to specify multiple modules that conflict will
result in shifter termination.

module_<name>_siteFs
--------------------
Module-specific external paths to be bind mounted into the container.  This
will allow additional external content, from the external OS or a shared
filesystem to be included, optionally, as part of the module.  All the warnings
and conditions applied to siteFs apply here as well.

module_<name>_copyPath
----------------------
The directory in the external environment that is to be copied to
/opt/udiImage/modules/<name>/

This can include libraries, scripts or other content that needs to be accessed
locally in the container.

module_<name>_enabled
---------------------
By default a module is enabled.  Setting enabled = 0 will prevent any container
from specifying or using it.  This parameter does not need to be specified in
most cases.

module_<name>_roothook
----------------------
Executes the configured script with /bin/sh as root, in the external environment
following most of the site container customization, including siteFs setup,
copyPath setup, and etc configuration, but before user content is introduced to
the container (in the non-overlay version of shifter).  This can be used to
perform some transformations of the container environment.  The current working
directory of the executed script is the container environment (be careful to use
relative paths from the CWD).  Non-zero exit status of the roothook terminates
container construction.

module_<name>_userhook
----------------------
Executes the configured script with /bin/sh as the user after container
construction, but immediately before a process is launched in the container
environment.  This may be useful to perform lightweight evaluations to
determine if the module is compatible with the user's container.  Non-zero
exit status of the userhook will terminate shifter execution before processes
are launched into the container.  The userhook can write to stdout or stderr to
communicate with the user.
Note: the userhook is run after the container has already lost access to
the external environment.  Recommend setting userhook to use a path in
/opt/udiImage/modules/<name> if module_<name>_copyPath is used, or use a siteFs
path.
