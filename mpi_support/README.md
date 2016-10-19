# Very hackish first MPI support implementation
This is a very quick and dirty first implementation of MPI support for Shifter.
It only works on Greina (tested on greina9 and greina10). It provides support for MVAPICH2.
The implementation consists of a hook script + some changes to the configuration of Shifter.

##udiRoot.conf
This is the Shifter configuration file. As far as MPI support is concerned, udiRoot.conf does three relevant things:

- Make Shifter execute mpi-support-hook.sh (see sitePreMountHook option).
- Make Shifter modify the LD_LIBRARY_PATH environment variable of the container in order that the container will use the libraries bind mounted by mpi-support-hook.sh.
- Make Shifter modify the PATH environment variable of the container in order that the container will use the binaries bind mounted by mpi-support.hook.sh.

##mpi-support-hook.sh
This script bind mounts required libraries and binaries from the host into the container.
