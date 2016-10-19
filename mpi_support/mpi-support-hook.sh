#!/bin/bash

exit_on_error()
{
    local error_code=$?
    local error_msg=$1
    if [ $error_code -ne 0 ]; then
        echo "MPI support ERROR: $error_msg"
        exit 1
    fi
}

bind_mount_folder()
{
    local target=$1
    local mount_point=$2

    mkdir -p $mount_point
    exit_on_error "cannot mkdir -p $mount_point"

    mount --bind $target $mount_point
    exit_on_error "cannot mount --bind $target $mount_point"
}

bind_mount_file()
{
    local target=$1
    local mount_point=$2
    local mount_dir=$(dirname $mount_point)

    mkdir -p $mount_dir
    exit_on_error "cannot mkdir -p $mount_dir"

    touch $mount_point
    exit_on_error "cannot touch $mount_point"

    mount --bind $target $mount_point
}

bind_mount_mpi_libraries()
{
    #mvapich
    bind_mount_folder "/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib" "/var/udiMount/opt/mpi-support/lib/mvapich2"
    bind_mount_file "/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpich.so" "/var/udiMount/opt/mpi-support/lib/misc/libmpich.so.12"

    #mellanox
    bind_mount_file "/lib64/libmlx5-rdmav2.so" "/var/udiMount/opt/mpi-support/lib/misc/libmlx5-rdmav2.so"
    bind_mount_file "/lib64/libmlx4-rdmav2.so" "/var/udiMount/opt/mpi-support/lib/misc/libmlx4-rdmav2.so"

    #misc
    bind_mount_file "/lib64/libxml2.so.2" "/var/udiMount/opt/mpi-support/lib/misc/libxml2.so.2"
    bind_mount_file "/lib64/libgpfs.so" "/var/udiMount/opt/mpi-support/lib/misc/libgpfs.so"
    bind_mount_file "/lib64/libibmad.so.5" "/var/udiMount/opt/mpi-support/lib/misc/libibmad.so.5"
    bind_mount_file "/lib64/librdmacm.so.1" "/var/udiMount/opt/mpi-support/lib/misc/librdmacm.so.1"
    bind_mount_file "/lib64/libibumad.so.3" "/var/udiMount/opt/mpi-support/lib/misc/libibumad.so.3"
    bind_mount_file "/lib64/libibverbs.so.1" "/var/udiMount/opt/mpi-support/lib/misc/libibverbs.so.1"
    bind_mount_file "/lib64/libnl.so.1" "/var/udiMount/opt/mpi-support/lib/misc/libnl.so.1"
}

bind_mount_mpi_binaries()
{
    bind_mount_folder "/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/bin" "/var/udiMount/opt/mpi-support/bin/mvapich2"
    bind_mount_folder "/cm/shared/apps/easybuild/software/GCC/5.3.0/bin" "/var/udiMount/opt/mpi-support/bin/gcc"
}

bind_mount_configurations()
{
    bind_mount_folder "/etc/libibverbs.d" "/var/udiMount/etc/libibverbs.d" 
}

bind_mount_mpi_libraries
bind_mount_mpi_binaries
bind_mount_configurations
