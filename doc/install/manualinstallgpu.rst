Manual installation of Shifter with GPU support
===============================================


Introduction: GPU support / Native performance for container images
-------------------------------------------------------------------


Containers allow applications to be portable accross platforms by packing the application in a complete filesystem that has everything needed for execution: code, libraries, system tools, environment variables, etc. Therefore, a container can ensure that an application always run under the same software environment.

To achieve a degree of portability, shifter (as the container runtime) has to provide the same interfaces and behavior independently of the hardware and target platform the container is executing on. This ensures that containerized CPU-based applications can be seamlessly deployed across platforms. However, this often means that hardware accelerated features that require specialized system drivers that are optimized for specific target system cannot be natively supported without breaking portability.

In order to mantain the portability that images provide while supporting site-optimized system features we implement a solution that relies on drivers that provide ABI compatibility and mount system specific features at the container's startup. In other words, the driver that is used by a container to allow the application to run on a laptop can be swapped at startup by the shifter runtime and instead an ABI compatible version is loaded that is optimized for the infrastructure of the supercomputer, therefore allowing the same container to achieve native performance.

We use this approach and solve the practical details for allowing container portability and native performance on a variety of target systems through command line options that indicate if, for instance, gpu support should be enabled and what gpu devices should be made available to the container image at startup.


Installation
============

Shifter Runtime
---------------

Most often, Shifter's runtime should be installed on the frontend node as well as on the compute nodes. However, it is also possible to install shifter solely on the compute nodes and use shifter on the frontend node through SLURM.


Dependencies
++++++++++++

Make sure you have all the required packages installed.
For RHEL/CentOS/Scientific Linux systems:

.. code-block:: bash

   yum install epel-release
   yum install gcc glibc-devel munge libcurl-devel json-c \
       json-c-devel pam-devel munge-devel libtool autoconf automake \
       gcc-c++ xfsprogs python-devel libcap-devel

For Debian/Ubuntu systems:

.. code-block:: bash

   sudo apt-get install unzip libjson-c2 libjson-c-dev libmunge2 libmunge-dev \
                        libcurl4-openssl-dev autoconf automake libtool curl \
                        make xfsprogs python-dev libcap-dev wget
 
  
Download, configure, build and install
++++++++++++++++++++++++++++++++++++++

Clone the github repository to obtain the source:

.. code-block:: bash

   git clone https://github.com/NERSC/shifter.git
   cd shifter

The following environment variables indicate the directories where Shifter's configuration files and images are located:

.. code-block:: bash

   export SHIFTER_SYSCONFDIR=/etc/shifter
   export UDIROOT_PREFIX=/opt/shifter/udiRoot

Configure and build the runtime:

.. code-block:: bash

   ./autogen.sh
   ./configure --prefix=$UDIROOT_PREFIX         \
               --sysconfdir=$SHIFTER_SYSCONFDIR \
               --with-json-c                    \
               --with-libcurl                   \
               --with-munge                     \
               --with-slurm=/path/to/your/slurm/installation
   make -j8
   sudo make install

Create links to system directories and additional required directories:

.. code-block:: bash

   sudo ln -s $UDIROOT_PREFIX/bin/shifter /usr/bin/shifter
   sudo ln -s $UDIROOT_PREFIX/bin/shifterimg /usr/bin/shifterimg
   sudo mkdir -p /usr/libexec/shifter
   sudo ln -s /opt/shifter/udiRoot/libexec/shifter/mount /usr/libexec/shifter/mount
   sudo mkdir -p $SHIFTER_SYSCONFDIR


Shifter's runtime configuration parameters
++++++++++++++++++++++++++++++++++++++++++

At run time, Shifter takes its configuration options from a file named *udiRoot.conf*. This file must be placed in the directory specified with *--sysconfdir* when running shifter's configure script. For reference, a template with a base configuration named *udiroot.conf.example* can be found inside the sources directory.

To illustrate the configuration process, consider the following parameters that were modified from the template configuration (*udiroot.conf.example*) to support the install on our local cluster named *Greina*:

* **imagePath=/scratch/shifter/images** Absolute path to shifter's images. This path must be readable by root and available from all nodes in the cluster.
* **etcPath=/etc/shifter/shifter_etc_files** Absolute path to the files to be copied into /etc on the containers at startup.
* **allowLocalChroot=1**
* **autoLoadKernelModule=0** Flag to determine if kernel modules will be loaded by Shifter if required. This is limited to loop, squashfs, ext4 (and dependencies). *Recommend value 0* if kernel modules (loop, squashfs, and ext4) are already loaded as part of the node boot process, otherwise use *value 1* to let Shifter load the kernel modules.
* **system=greina** The name of the computer cluster where shifter is deployed. It is **important for this to match the platform name in the json configuration file** for the Image Manager.
* **imageGateway=http://greina9:5000** Space separated URLs for where the Image Gateway can be reached.
* **siteResources=/opt/shifter/site-resources** Absolute path to where site-specific resources will be bind-mounted inside the container to enable features like native MPI or GPU support. This configuration only affects the container. The specified path will be automatically created inside the container. The specified path doesn't need to exist on the host.


Shifter Startup
+++++++++++++++

As mentioned earlier, the Shifter runtime requires the loop, squashfs, ext4 kernel modules loaded. If these modules are not loaded automatically by shifter, they can be loaded manually with:

.. code-block:: bash

   sudo modprobe ext4
   sudo modprobe loop
   sudo modprobe squashfs



Image Gateway
-------------

The Image Gateway can run on any node in your cluster. The requirement for the Image Gateway are:

* munge must be running.
* its using the same munge key as all login and compute nodes.
* all nodes can access the imagegwapi address and port as indicated in Shifter's configuration file.

Software dependencies for the Image Gateway
+++++++++++++++++++++++++++++++++++++++++++

The Image Gateway depends on *MongoDB* server, *Redis*, *squashfs-tools*, virtualenv (to further install all other python dependencies on a virtual environment), and python2.7. It is recommended to also install the dependencies needed by the shifter runtime, as of this time we have not verified which of Shifter's dependencies can be omitted as they are not needed by the image gateway.

For RHEL/CentOS/Scientific Linux systems:

.. code-block:: bash

   yum install mongodb-server redis squashfs-tools


For Debian/Ubuntu systems:

.. code-block:: bash

   sudo apt-get install mongodb redis-server squashfs-tools

Install *virtualenv* through  *pip* for Python:

.. code-block:: bash

   wget https://bootstrap.pypa.io/get-pip.py
   sudo python get-pip.py
   sudo pip install virtualenv


Installation of the Image Gateway
+++++++++++++++++++++++++++++++++
We need to create three directories:

1. Where to install the Image Gateway
2. Where the Image Gateway will cache images
3. Where the Image Gateway will expand images. **Note: For performance reasons this should be located in a local file system** (we experienced a **40x** slowdown of pulling and expanding images when the images were expanded on a Lustre parallel file system!).

.. code-block:: bash

   export IMAGEGW_PATH=/opt/shifter/imagegw
   export IMAGES_CACHE_PATH=/scratch/shifter/images/cache
   export IMAGES_EXPAND_PATH=/var/shifter/expand
   mkdir -p $IMAGEGW_PATH
   mkdir -p $IMAGES_CACHE_PATH
   mkdir -p $IMAGES_EXPAND_PATH

Copy the contents of *shifter-master/imagegw* subdirectory to *$IMAGEGW_PATH*:

.. code-block:: bash

   cp -r imagegw/* $IMAGEGW_PATH


Next step is to prepare a python virtualenv in the Image Gateway installation directory. If this directory is owned by root, the virtualenv and the python requirements need to be also installed as root.


**Note**

* Installing packages in the virtualenv as a regular user using `sudo pip install` will override the virtualenv settings and install the packages into the system's Python environment.
* Creating the virtualenv in a different folder (e.g. your `/home` directory), installing the packages and copying the virtualenv folder to the Image Gateway path will make the virtualenv refer to the directory where you created it, causing errors with the workers and configuration parameters.


.. code-block:: bash

   cd $IMAGEGW_PATH
   # Install the virtualenv and all python dependencies as root
   sudo -i
   # Set the interpreter for the virtualenv to be python2.7
   virtualenv python-virtualenv --python=/usr/bin/python2.7
   source python-virtualenv/bin/activate
   # The requirement file should already be here if the imagegw folder has been copied
   # from the Shifter sources
   pip install -r requirements.txt
   deactivate
   # If you switched to root, return to your user
   exit


Clone and extract the rukkal/virtual-cluster repository from Github:


.. code-block:: bash

   wget https://github.com/rukkal/virtual-cluster/archive/master.zip
   mv master.zip virtual-cluster-master.zip
   unzip virtual-cluster-master.zip


Copy the following files from the virtual-cluster installation resources:

.. code-block:: bash

   cd virtual-cluster-master/shared-folder/installation
   sudo cp start-imagegw.sh ${IMAGEGW_PATH}/start-imagegw.sh
   sudo cp init.d.shifter-imagegw /etc/init.d/shifter-imagegw


Configure
+++++++++

For configuration parameters, the Image Gateway uses a file named *imagemanager.json*. The configuration file must be located in the directory that was specified in Shifter's *$SHIFTER_SYSCONFDIR* (*--sysconfdir* when running shifter's *configure* script). A base template file named *imagemanager.json.example* can be found inside the sources directory.

As a reference of configuration parameters consider the following entries as they were used when installing in our local cluster (Greina):

* **"CacheDirectory": "/scratch/shifter/images/cache/"**: Absolute path to the images cache. The same you chose when defining **$IMAGES_CACHE_PATH**
* **"ExpandDirectory": "/var/shifter/expand/"**: Absolute path to the images expand directory. The same you chose when defining **$IMAGES_EXPAND_PATH**.
* Under **"Platforms"** entry change **"mycluster"** to the name of your system. This should be the same name you set for system in *udiRoot.conf*.
* **"imageDir": "/scratch/shifter/images"**: This is the last among the fields defined for your platform. It is the absolute path to where shifter can find images. Should be the same as *imagePath* in *udiRoot.conf*.

Save the file to a local copy (e.g. *imagemanager.json.local*, just to have a backup ready for your system) and copy it to the configuration directory:

.. code-block:: bash

   sudo cp imagemanager.json.local $SHIFTER_SYSCONFDIR/imagemanager.json

Lastly, open *$IMAGEGW_PATH/start-imagegw.sh* and enter the name of your system in the line. This will spawn Celery worker threads with a more identifiable name.

.. code-block:: bash

   SYSTEMS="mycluster"


Image Gateway Startup
+++++++++++++++++++++

Start the services for Redis and MongoDB:

.. code-block:: bash

   sudo systemctl start redis
   sudo systemctl start mongod

Start the Shifter Image Gateway:

.. code-block:: bash

   sudo /etc/init.d/shifter-imagegw start















