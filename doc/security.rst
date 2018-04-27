Security Considerations with Shifter
====================================

WARNING: Shifter use a great deal of root privilege to setup the container
environment.  The "shifter" executable is setuid-root, and when run with batch
integration the setupRoot/unsetupRoot utilities must be run as root.  We are
working to reduce the privilege profile of starting Shifter containers to
reduce risk as much as possible.

Once a process is launched into the container, processes are stripped of all
privilege, and should not be able to re-escalate afterwards.

Shifter enables User Defined Image environment containers.  To do this while
optimizing I/O on the compute nodes it does require performing several
privileged operations on the execution node, both privilege to mount filesystems
and rewrite the user space, and privilege to manipulate devices on the system.

Furthermore, because the environment is _user defined_, it is possible that a
user could include software which could generate security vulnerabilities if
a privileged process accesses such software or resources.

Therefore, to mitigate potential risks, we recommend the following:

1. Avoid running privileged processes in containers.  This means both explicitly
   chroot'ing into a container as root, or joining the namespace of an existing
   container, as well as preventing setuid root applications to operate *at all*
   within the container.

   On more recent systems, Shifter will attempt to permanently drop privileges
   using the "no_new_privs" process control setting, see:
   https://www.kernel.org/doc/Documentation/prctl/no_new_privs.txt

   See the :doc:`sshd` document for more information on the Shifter-included
   sshd and recommendations around running it as root (don't unless you must).

2. Related to point one, preventing setuid-root applications from operating with
   privilege is mostly achieved through mounting as much as possible within the
   Shifter environment "nosuid", meaning the setuid bits on file permissions are
   ignored.  In addition, processes started within shifter and their children are
   prevented from ever gaining additional privileges by restricting the set of
   capabilities they can acquire to the null set.

   One exception to the nosuid ban is if the ":rec" or ":shared" siteFs mount
   flags are used.  The recursive bind mount operation will copy the mount flags
   from the base system, and will not follow Shifter standards.  Similarly, the
   "shared" mount propagation strategy will remove the mounts from Shifter's
   strict control.  The privilege capability restrictions should prevent
   processes from escalating privilege even without the nosuid restriction.

   Thus, if you operate the sshd as root, *DO NOT* use the recursive mount
   option or shared if there is _any_ chance that there are setuid-root (or
   other privileged user) files mounted under the target path, or executables
   that grant specific security capabilities.  The ":rec" or ":shared" options
   can be a very powerful feature, however *USE WITH GREAT CAUTION* if you allow
   the sshd to operate with root privilege.


3. Use the most recent version of Shifter (16.08) as it corrects some issues
   from the previous pre-releases.

Notes on Security Related Options and Future Directions for udiRoot
-------------------------------------------------------------------
Shifter attempts to provide an HPC Environment Container solution for a variety
of HPC platforms.  This means a wide variety of Linux kernels must be supported.

A note on User Namespaces:  We have considered the use of user namespaces, and
may make more use of them in the future.  At the present, however, too few of
the target Linux distributions support them or support them well to make the
investment worthwhile.  Also there have been a number of security issues
surrounding uid and gid mapping so as not to make it a safe and reliable solution
until quite recent linux kernels.  The promise of user/group mapping will help to
ensure the safety of running software from potentially hostile/insecure
environments, in particular helping to relax the constraints around the
prohibition of setuid files.

A note on loop devices:  Shifter makes heavy use of loop devices owing to the
great performance of benefits optimizing the balance of local node memory and
network bandwidth consumption, while avoiding some of the performance issues
caused by distributed locking in traditional network filesystems.  The use of
these loop devices, however, means that filesystems are being managed and accessed
directly by the kernel, with privileged access.  This means that the filesystem
files *must never be writable by users directly*, and should be produced by
toolsets trusted by the site operating Shifter.  Directly importing ext4, xfs,
or even squashfs filesytems should be avoided unless you trust the individual
that produced the content with root privileges. (You wouldn't pick up a USB
drive off the street and put it into your computer, would you?)

Notes on Security attributes
----------------------------
Linux supports the ability for file systems to have security attributes attached
to individual files.  This allows, for example, ping to be non-setuid yet still
create raw packets.  This can introduce additional risk with running containers.
Shifter takes a number of precautions to mitigate this risks.  The Image gateway
uses a python tar library that doesn't currently support extracting files with
the security attribute.  In addition, we recommend running the gateway as a
non-root users, e.g. shifter.  This adds additional security because an unprivileged
user cannot add security attributes to a file.  Shifter passes flags to
mksquashfs to further prevent these attributes from being included in the image.
Finally, the runtime uses a number of mechanisms to ensure these attributes are
ignored.  These combination of features greatly reduce the risk of a unprivileged
user from using specially crafted images to gain privileges.

Users making use of the custom image import option (see import.md) should take
additional precautions since this mechanism effectively bypasses the image
preparation steps in the gateway and the images could include security
attributes.  It is recommended that sites build these images using a
unprivileged account and pass the --no-xattrs flag to mksquashfs to mitigate
this risk.  The runtime should still prevent these images from conferring any
additional privileges to the user process, but dropping the attributes during
preparation is a good precaution.  In addition, sites should limit the users
allowed to perform imports.

Image Manager Considerations
----------------------------

In general, the image gateway does not require any privileges to performs its
functions.  The gateway does control what images are made available to the
system, but importing these images can be done without special privileges.

User Authentication
+++++++++++++++++++

Access to the image gateway API service requires authentication using Munge.
Munge is a light-weight, shared key authentication system that allows a privileged
daemon process to authenticate the user who is calling the process.

User authentication is used in several way.  It is used to verify the user before
performing some restricted administrative functions (e.g. image expiration) and
to prevent external clients from triggering operations that could impact response
times.  In addition, with the introduction of image ACLs, user authentication
is used to limit access to restricted images based on the desired access controls
of the image owner(s).

Securing imagegw api
++++++++++++++++++++

1. run as a non-root user using gunicorn as the python flask host
2. Use a firewall to restrict traffic just to the networks that need to access the
   imagegw
3. install the mksquashfs, ext3/4 utilities and xfs progs in a trusted way (e.g.,
   package manager of your distribution)

Running the imagegw as a non-root user is particularly important to
ensure images generated do not have an Linux security capabilities embedded in
the image.  This is a non-obvious way that a program may attempt to escalate
privilege.  On more recent Linux systems (Linux kernel >= 3.5), this risk is
somewhat mitigated so long as the shifter executable is rebuilt for those
systems.
