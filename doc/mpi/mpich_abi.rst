MPI Support in Shifter: MPICH abi
=================================
MPICH and its many variants agreed in 2014 to retain ABI compatibility to
help improve development practices.  However, this ABI compatibility also
provides a clear path for almost transparently supporting MPI within shifter
environment containers.

The basic idea is that the container developer will use a fairly vanilla
version of MPICH and dynamically link their application against that.  The
shifter-hosting site then configures shifter to inject their site-specific
version of MPICH (perhaps a Cray, Intel, or IBM variant) linked to the
interconnect and workload manager driver libraries.  The site-specific version
of libmpi.so then overrides the version in the container, and the application
automatically uses it instead of the generic version originally included in the
container.

Container Developer Instructions
--------------------------------
Here is an example Dockerfile::

    FROM ubuntu:14.04
    RUN apt-get update && apt-get install -y autoconf automake gcc g++ make gfortran
    ADD http://www.mpich.org/static/downloads/3.2/mpich-3.2.tar.gz /usr/local/src/
    RUN cd /usr/local/src/ && \
        tar xf mpich-3.2.tar.gz && \
        cd mpich-3.2 && \
        ./configure && \
        make && make install && \
        cd /usr/local/src && \
        rm -rf mpich-3.2

    ADD helloworld.c /
    RUN mkdir /app && mpicc helloworld.c -o /app/hello

    ENV PATH=/usr/bin:/bin:/app

Going through the above:

1. base from a common distribution, e.g., ubuntu:14.04,
2. install compiler tools to get a minimal dev environment.
3. get and install mpich 3.2
4. add and compile your application
5. Setup the environment to easily access your application

To construct the above container, one would do something like::

    docker build -t dmjacobsen/mpitest:latest .

(setting your tag appropriately, of course)


SLURM User Instructions
-----------------------
If the MPICH-abi environment is configured correctly (see below), it should be
very easy to run the application.  Building from the example above::

    dmj@cori11:~> shifterimg pull dmjacobsen/mpitest:latest
    2016-08-05T01:14:59 Pulling Image: docker:dmjacobsen/mpitest:latest, status: READY
    dmj@cori11:~> salloc --image=dmjacobsen/mpitest:latest -N 4 --exclusive
    salloc: Granted job allocation 2813140
    salloc: Waiting for resource configuration
    salloc: Nodes nid0[2256-2259] are ready for job
    dmj@nid02256:~> srun shifter hello
    hello from 2 of 4 on nid02258
    hello from 0 of 4 on nid02256
    hello from 1 of 4 on nid02257
    hello from 3 of 4 on nid02259
    dmj@nid02256:~> srun -n 128 shifter hello
    hello from 32 of 128 on nid02257
    hello from 46 of 128 on nid02257
    hello from 48 of 128 on nid02257
    hello from 55 of 128 on nid02257
    hello from 57 of 128 on nid02257
    ...
    ...
    hello from 26 of 128 on nid02256
    hello from 27 of 128 on nid02256
    hello from 28 of 128 on nid02256
    hello from 29 of 128 on nid02256
    hello from 30 of 128 on nid02256
    hello from 31 of 128 on nid02256
    dmj@nid02256:~> exit
    salloc: Relinquishing job allocation 2813140
    salloc: Job allocation 2813140 has been revoked.
    dmj@cori11:~>


System Administrator Instructions: Configuring Shifter
------------------------------------------------------
The basic plan is to gather the libmpi.so* libraries and symlinks and copy them
into the container at runtime.  This may require some dependencies to also be
copied, but hopefully only the most limited set possible.  The current
recommendation is to copy these libraries into /opt/udiImage/<type>/lib64, and
all the dependencies to /opt/udiImage/<type>/lib64/dep

We then use patchelf to rewrite the rpath of all copied libraries to point to
/opt/udiImage/<type>/lib64/dep

The source libraries must be prepared ahead of time using one of the helper
scripts provided in the extras directory, or a variant of same. As we get
access to different types of systems, we will post more helper scripts and
system-type-specific instructions.

Finally, we need to force LD_LIBRARY_PATH in the container to include
/opt/udiImage/<type>/lib64

Cray
++++
Run the `prep_cray_mpi_libs.py` script to prepare the libraries::

   login$ python /path/to/shifterSource/extra/prep_cray_mpi_libs.py /tmp/craylibs

Note: in CLE5.2 this should be done on an internal login node; in CLE6 an
internal or external login node should work. You'll need to install patchelf
into your PATH prior to running (https://nixos.org/patchelf.html)

Next copy /tmp/craylibs to your shifter module path (see Modules) under
mpich/lib64, e.g., :code:`/usr/lib/shifter/modules/mpich/lib64`.

Finally, a few modifications need to be made to udiRoot.conf:

1. add "module_mpich_siteEnvPrepend = LD_LIBRARY_PATH=/opt/udiImage/modules/mpich/lib64"
2. add "module_mpich_copyPath = /usr/lib/shifter/modules/mpich"
3. add "/var/opt/cray/alps:/var/opt/cray/alps:rec" to siteFs
4. if CLE6, add "/etc/opt/cray/wlm_detect:/etc/opt/cray/wlm_detect" to siteFs
5. add "defaultModules = mpich" to load cray-mpich support by default in all containers

Note, you may need to modify your sitePreMountHook script to create
/var/opt/cray and /etc/opt/cray prior the mounts.

Instead of setting up the module_mpich_copyPath, you could use siteFs to bind-mount
the content into the container instead, which may have performance benefits in some
environments, e.g. set module_mpich_siteFs = /usr/lib/shifter/modules/mpich:/shifter/mpich.
In that case you'll need to adjust the module_mpich_siteEnvPrepend paths, and pre-create
the /shifter directory using the sitePreMountHook.

------

Other MPICH variants/vendors coming soon.  If you have something not listed
here, please contact shifter-hpc@googlegroups.com!
       


[1] https://www.mpich.org/abi/
