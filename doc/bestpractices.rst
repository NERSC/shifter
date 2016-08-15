Shifter Recommended Practices
=============================

Recommended Practices for Container Developers
----------------------------------------------


Recommended Practices for Users
-------------------------------


Recommended Practices for System Administrators
-----------------------------------------------

0. Avoid running privileged processes, as much as possible, in the User
   Defined Image environments.  This is because the user-defined image is
   prepared externally, and may or may not contain security vulnerabilities
   fixed or otherwise not present in your environment.  Shifter does optionally
   include an sshd which is also *not* recommended to be run as root, however,
   on some legacy systems it may be necessary. Therefore, we have attempted to
   secure the sshd by statically linking it against libmusl (an embedded libc
   alternative), and libressl, to minimize to number and types of interactions
   it might have with the user defined environment.

1. Avoid race conditions in shifter startup by pre-creating udiMount and
   loopMount (paths defined in udiRoot.conf).  If, /var/udiMount, if that is
   configured as your udiMount path, does not exist, shifter will attempt to
   create it.  If a user starts a parallel application without WLM support
   (not recommended), all copies will attempt to create this directory, which
   could lead to a possible race.  Avoid this with::
   
      mkdir /var/udiMount
      mkdir /var/loopMount

   as part of your node bootup.

2. Pre-load all of the shifter-required kernel modules as part of system boot.
   For legacy reasons, which will be removed in a future release, shifter can be
   configured to auto-load certain kernel modules.  It is recommended to avoid
   this and simply pre-load all the kernel modules your instance of shifter
   might need.  Which modules this may be entirely depends on your site kernel
   configuration.

   An example might be::
   
       modprobe loop max_loop=128
       modprobe squashfs
       modprobe xfs ## (optional, for perNodeCache)

3. Ensure there are plenty of loop devices available.  I recommend at least 2x
   more than the expected number of independent shifter instances you plan on
   allowing per node.  How this is configured is dependent upon how your kernel
   is built, whether the loop device is compiled-in or presented as a kernel
   module.  If the loop device is compiled-in, you'll need to set
   max_loop=<number> as a boot kernel argument.  If it is compiled as a kernel
   module, you'll need to specify max_loop=<number> to insmod or modprobe when
   it is loaded.

4. Make your parallel/network filesystems available in user containers!  This is
   done by setting the siteFs variable in udiRoot.conf.  Note that any path you
   add will be done _prior_ to adding user content to the image, and it will
   obscure user content if there is a conflict anywhere along the path.

   e.g., if you have a parallel filesystem mounted on /home, adding::

       siteFs=/home:/home

   will mount your /home into all user containers on the /home mountpoint;
   however no content the user defined image might have in /home will be
   accessible.

   It is strongly recommended to mount your parallel filesystems on the same
   path users are accustomed to if at all possible, (e.g. mount /home on /home,
   /scratch on /scratch, etc).  If this might obscure important user content,
   for example if you have a parallel filesystem mounted under /usr, then you
   must change the shifter mount points to avoid damaging user content.

5. Use the pre and post mount hooks to customize content to match your site.
   When shifter creates the siteFs-specified mount points, it will attempt
   a simple "mkdir", not the equivalent of "mkdir -p", so, if for example,
   you have a siteFs like::

       siteFs=/home:/home;\
              /global/project:/global/project;\
              /global/common:/global/common;
              /var/opt/cray/alps:/var/opt/cray/alps

   Your sitePreMountHook script might look like::

       #!/bin/bash
       mkdir global
       mkdir -p var/opt/cray

   This is because shifter can create home trivially, but the paths require
   some setup.  Note that the script is executed in the udiMount cwd, however
   it is _not_ chroot'd into it, this allows you to use content from the
   external environment, however, means you should be very careful to only
   manipulate the udiMount, in this example, that means doing
   `mkdir global`, not `mkdir /global`

   You can use the sitePostMountHook script to do any final setup before the
   user content is added to the image.  For example, if your site usually
   symlinks /global/project to /project and you want that available in the
   shifter containers, your sitePostMountHook script might look like::

       #!/bin/bash
       ln -s global/project project

6. Use the optUdiImage path to add content to user images at runtime.  The path
   specified for optUdiImage in udiRoot.conf will be recursively copied into the
   image on /opt/udiImage.  This is a great way to force content into an
   out-of-the-way path at runtime, however, since it is copied (instead of bind-
   mounted, like siteFs), it is intended to be used for performance-sensitive
   executables. The optional sshd is in this path, and if any of the optional 
   site-specific MPI support is desired, this should be copied to this path as
   well.
