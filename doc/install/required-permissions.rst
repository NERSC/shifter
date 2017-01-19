Summary of required permissions for installation and execution
==============================================================

Shifter Runtime installation
-------------------------------------

* Write permissions to:
    - A directory for configuration files. This is passed as the ``--sysconfdir``
      option to the configure script. The typical value is ``/etc/shifter``.
    - A directory where udiRoot will be installed. This will be passed as the
      ``--prefix`` option to the configure script. Typical values are
      ``/opt/shifter/default`` or ``/opt/shifter/udiRoot``.
    - A directory where the shifter executable loop mounts the image. This is
      selected with the ``loopMount`` parameter in the ``udiRoot.conf`` file.
      The typical value is ``/var/udiLoopMount``.
    - A path where to create the root directory for the containerized application.
      This is selected with the ``udiMount`` parameter in the ``udiRoot.conf`` file.
      The typical value is ``/var/udiMount``.
    - ``/usr`` (for symlinks to executables and Shifter's ``udiRoot/libexec`` directory)
    - A directory for storing container images. This path must be readable by root
      and available from all nodes in the cluster.
      This is selected with the ``imagePath`` parameter in the ``udiRoot.conf`` file.


Shifter Runtime startup/run
---------------------------

* Shifter must run as a SUID executable and be able to achieve full root
  privileges to perform mounts

* Load the following kernel modules:
    - ext4
    - loop
    - squashfs
    - nvidia
    
    These modules have to be loaded in order for Shifter to work.
    Shifter can be configured to automatically load at runtime the kernel modules
    related to the filesystem selected as the image format. However, this option is
    disabled by default.


Image Gateway installation
--------------------------

* Write permissions to:
    - A directory where the Image Gateway will be installed.
      The typical value is ``/opt/shifter/imagegw``.
    - A directory to cache images during downloads from Docker Hub (e.g. ``/var``
      or a scratch filesystem).
      This is selected with the ``CacheDirectory`` parameter in the ``imagemanager.json``
      file.
    - A directory where to expand images before conversion to Shifter's format.
      This is selected with the ``ExpandDirectory`` parameter in the ``imagemanager.json``
      file.
      *Note*: For performance reasons this should be located in a local file system
      (we experienced a **40x** slowdown of pulling and expanding images when
      the images were expanded on a Lustre parallel file system!).
    - ``/etc`` (for copying init.d scripts)

* If the Image Gateway is installed in a directory owned by root, root privileges
  are required in order to create a Python virtual environment in the same directory.


Image Gateway execution
-----------------------

* Availability of the following software:
    - munge
    - mongoDB
    - Redis
    - Squashfs tools
* Munge, mongoDB and Redis services must be running
* The Image Gateway node is using the same munge key as all login and compute
  nodes
* All nodes can access the imagegwapi address and port as indicated in Shifter's configuration file
* Write permissions to ``/var`` when starting up the Image Gateway API service (for 
  creating lockfile, pidfile and log).
* If the Python virtual environment was installed with root privileges, superuser
  permissions are required to startup the Image Gateway API service and the Celery
  distributed worker queue.