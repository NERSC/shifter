The shifter sshd
================

The Workload Manager integration can cause an sshd to be started within each
instance of the container.  This feature is useful for allowing complex
workflows to operate within the User Defined environment, and operate as if
they were running on a cluster running the User Defined image.

How to use it: User Perspective
-------------------------------
To use the shifter ssh access, the "basic" way is to start a batch job
specifying a shifter image.  So long as your job has exclusive access to the
compute nodes in its allocation, the shifter container should be setup, and
be pre-loaded with the running sshd.  You are the only user that can login via
this sshd.

Once your job is running, enter the shifter environment with the shifter
command, e.g., :code:`shifter /bin/bash`

Once in the shifter environment you should be able to simply `ssh` to any
other node in your allocation, and remain within the container environment.

Note, this works because of a special ssh configuration loaded into your
container environment at runtime.  If your home directory has a
:code:`~/.ssh/config` it may override some of the necessary shifter sshd
settings, in particular if you override IdentityFile for all hosts.

With a complete implementation of the batch system integration, you should be
able to get a complete listing of all the other hosts in your allocation by
examining the contents of :code:`/var/hostsfile` within the shifter 
environment.

TODO: To be continued...

How to configure it: Administrator Perspective
----------------------------------------------
If you didn't disable the build of the sshd when shifter was compiled, it would
be installed in the udiImage directory
(:code:`%(libexec)/shifter/opt/udiImage`).  The udiImage directory can be moved
onto your parallel/network storage to ease maintenance.  Just make sure to 
update the udiRoot.conf :code:`udiImagePath` to reflect the change.  Under
:code:`udiImage/etc` you'll find the default sshd_config and ssh_config files.
The can be modified to suit your needs, however they have been pre-configured
to attempt to meet most ssh needs of the shifter containers, namely:

1. Disable privileged login
2. Only allow the user to login
3. Only permit the user's udiRoot ssh key to login to the sshd
4. Intentionally omits an ssh host key as these are dynamically generated
   and accessed within the container environment

When the container is setup, the :code:`udiImagePath` will be copied to
:code:`/opt/udiImage` within the container.  The ssh installation is
based using this path.

TODO: To be continued...

How to configure it: WLM Integrator Perspective
-----------------------------------------------
TODO: Not started yet.


Nitty Gritty Details
--------------------
TODO: Not started yet.

Can I use the user sshd in Cray CLE5.2UP03 or UP04?
+++++++++++++++++++++++++++++++++++++++++++++++++++
Yes, however the default environment does not (or did not) support it
out-of-the-box.  You'll need a few things:

1. shifter 16.08.3 or newer
2. 5.2UP04 or 5.2UP03 fully patched for the glibc issues earlier in 2016
3. A modified compute node /init

The compute node /init needs to be updated to mount /dev/pts with proper
options to allow pty allocation in the absence of pt_chown, e.g., replace::

mkdir -p "$udev_root/pts"
mount -t devpts devpts "$udev_root/pts"

with::

mkdir -p "$udev_root/pts"
mount -t devpts -o gid=5,mode=620 devpts "$udev_root/pts"


