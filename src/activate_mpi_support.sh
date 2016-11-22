#!/bin/bash

site_mpi_libraries="
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.a
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.so
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.so.12
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.so.12.0.5

/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpicxx.a
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpicxx.so
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpicxx.so.12
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpicxx.so.12.0.5

/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpifort.a
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpifort.so
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpifort.so.12
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpifort.so.12.0.5

/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpl.so

/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpich.so

/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libfmpich.so
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpichf90.so

/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpichcxx.so

/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libopa.so"

site_libraries="
/lib64/libmlx5-rdmav2.so
/lib64/libmlx4-rdmav2.so
/lib64/libxml2.so.2
/lib64/libgpfs.so
/lib64/libibmad.so.5
/lib64/librdmacm.so.1
/lib64/libibumad.so.3
/lib64/libibverbs.so.1
/lib64/libnl.so.1"

site_binaries="
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/hydra_nameserver
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/hydra_pmi_proxy
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpicc
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpicxx
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpiexec.hydra
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpif77
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpifort
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpirun
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpispawn
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/parkill
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/hydra_persist
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpic++
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpichversion
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpiexec
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpiexec.mpirun_rsh
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpif90
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpiname
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpirun_rsh
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin/mpivars"

site_configuration_files="
/etc/libibverbs.d/mlx4.driver
/etc/libibverbs.d/mlx5.driver"

#here is necessary to set PATH manually because shifter executes
#this script with an empty environment
export PATH=/usr/local/bin:/usr/bin:/bin:/sbin

container_mpi_dir=/var/udiMount/site-resources/mpi
container_lib_dir=/var/udiMount/site-resources/mpi/lib

log()
{
    local level="$1"
    local msg="$2"
    printf "[ MPI SUPPORT ACTIVATION ] =$level= $msg\n" >&2
}

exit_if_previous_command_failed()
{
    local error_code=$?
    local error_msg="$1"
    if [ $error_code -ne 0 ]; then
        log ERROR "$error_msg"
        exit $error_code
    fi
}

parse_command_line_arguments()
{
    if [ $# -eq 3 ]; then
        local site_resources_dir="$1"
        #TODO: initialize lib, bin dir variables
    else
        log ERROR "internal error: received bad number of command line arguments"
        exit 1
    fi
}

validate_command_line_arguments()
{
    if [ ! -d $container_mount_point ]; then
        log ERROR "internal error: received invalid value for 'mount point' command line argument"
        exit 1
    fi
}

bind_mount_folder()
{
    local target=$1
    local mount_point=$2

    mkdir -p $mount_point
    exit_if_previous_command_failed "internal error: cannot mkdir -p $mount_point"

    mount --bind $target $mount_point
    exit_if_previous_command_failed "internal error: cannot mount --bind $target $mount_point"
}

bind_mount_file()
{
    local target=$1
    local mount_point=$2
    local mount_dir=$(dirname $mount_point)

    mkdir -p $mount_dir
    exit_if_previous_command_failed "cannot mkdir -p $mount_dir"

    touch $mount_point
    exit_if_previous_command_failed "cannot touch $mount_point"

    mount --bind $target $mount_point
}

bind_mount_files()
{
    local targets="$1"
    local mount_dir="$2"

    mkdir -p $mount_dir
    exit_if_previous_command_failed "cannot mkdir -p $mount_dir"

    for target in $targets
    do
        local mount_point=$mount_dir/$(basename $target)

        touch $mount_point
        exit_if_previous_command_failed "cannot touch mount point $mount_point"

        mount --bind $target $mount_point
        exit_if_previous_command_failed "cannot bind mount $target to $mount_point"
    done
}

bind_mount_mpi_libraries()
{
    bind_mount_files "$site_libraries" "$container_lib_dir"
    bind_mount_files "$site_mpi_libraries" "$container_lib_dir"
    bind_mount_file "/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpich.so" "$container_lib_dir/libmpich.so.12"
}

bind_mount_mpi_binaries()
{
    #TODO COMMENT THIS bind_mount_folder "/cm/shared/apps/easybuild/software/GCC/5.3.0/bin" "/var/udiMount/opt/mpi-support/bin/gcc"
    bind_mount_files "$site_binaries" "$container_mpi_dir/bin"
}

bind_mount_configuration_files()
{
    bind_mount_files "$site_configuration_files" "/var/udiMount/etc/libibverbs.d"
}

#parse_command_line_arguments
#validate_command_line_arguments
bind_mount_mpi_libraries
bind_mount_mpi_binaries
bind_mount_configuration_files
