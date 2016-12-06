#!/bin/bash

# TODO: expose static libraries in the container too? It only makes sense
# if the user needs to build in the running container.
site_mpi_static_libraries="
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.a
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpicxx.a
/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpifort.a"

# This is a list of key-value pairs defined in the form "<key1>:<value1>..."
# The key is the name of the container's library to be substituted.
# The value is the full path of the site's library that will substitute the container's library.
#
# The MPI support machinery will check that the site's library is ABI compatible with the
# container's library to be substituted. The compatibility check is performed by comparing the
# version numbers specified in the libraries' file names as follows:
# - The major numbers (first from the left) must be equal.
# - The site's minor number (second from the left) must be greated or equal to the container's minor number.
# - If the site's library name doesn't contain any version number, no compatibility check is performed.
# This compatibility check is compatible with the MPICH ABI version number schema.
site_mpi_shared_libraries="
libmpi.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.so.12.0.5
libmpicxx.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpicxx.so.12.0.5
libmpifort.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpifort.so.12.0.5
libmpl.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.so
libopa.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.so
libmpich.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpi.so.12.0.5
libmpichcxx.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpicxx.so.12.0.5
libmpichf90.so:/cm/shared/apps/easybuild/software/MVAPICH2/2.2b-GCC-5.3.0/lib/libmpifort.so.12.0.5"

# This is a list of libraries that are dependencies of the site MPI libraries.
# These libraries are always bind mounted in the container when the MPI support is active.
site_mpi_dependency_libraries="
/lib64/libmlx5-rdmav2.so
/lib64/libmlx4-rdmav2.so
/lib64/libxml2.so.2
/lib64/libgpfs.so
/lib64/libibmad.so.5
/lib64/librdmacm.so.1
/lib64/libibumad.so.3
/lib64/libibverbs.so.1
/lib64/libnl.so.1"

# This is a list of site MPI command line tools that will be bind mounted in the container.
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

# This is a list of site configuration files that will be copied in the container.
site_configuration_files="
/etc/libibverbs.d/mlx4.driver
/etc/libibverbs.d/mlx5.driver"

#here is necessary to set PATH manually because shifter executes
#this script with an empty environment
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin

container_root_dir=
container_lib_dir=
container_bin_dir=

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
    if [ ! $# -eq 2 ]; then
        log ERROR "internal error: received bad number of command line arguments"
        exit 1
    fi

    container_root_dir=$1
    if [ ! -d $container_root_dir ]; then
        log ERROR "internal error: received bad container root directory ($container_root_dir)"
        exit 1
    fi

    local site_resources=$2
    if [ ! -d $container_root_dir/$site_resources ]; then
        log ERROR "internal error: received unexisting site-resources folder ($site_resources)"
        exit 1
    fi
    container_lib_dir=$container_root_dir$site_resources/mpi/lib
    container_bin_dir=$container_root_dir$site_resources/mpi/bin
}

check_that_image_contains_required_dependencies()
{
    # we need the container image to provide these command line tools
    command_line_tools="sed realpath"
    for command_line_tool in $command_line_tools; do
        if [ -z $(chroot $container_root_dir bash -c "which $command_line_tool") ]; then
            log ERROR "missing dependency: make sure that the container image contains the program \"$command_line_tool\""
            exit 1
        fi
    done
}

check_that_image_contains_mpi_libraries()
{
    local container_lib_realpaths=$(chroot $container_root_dir bash -c "ldconfig -p | sed -n -e 's/.* => \(.*\)/echo \$(realpath \1)/p' | bash")

    for container_lib_realpath in $container_lib_realpaths; do
        if corresponding_site_mpi_library_exists $container_lib_realpath; then
            return # the image contains an MPI library, we are good
        fi
    done

    log ERROR "No MPI libraries found in the container. \
The container should be configured to access the MPI libraries through the dynamic linker. \
Hint: run 'ldconfig' inside the container to configure the dynamic linker \
and run 'ldconfig -p' to check the configuration."
    exit 1
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

    for target in $targets; do
        local mount_point=$mount_dir/$(basename $target)

        touch $mount_point
        exit_if_previous_command_failed "cannot touch mount point $mount_point"

        mount --bind $target $mount_point
        exit_if_previous_command_failed "cannot bind mount $target to $mount_point"
    done
}

# drop parent folders and trailing version numbers from the given library name
strip_library_name()
{
    local lib_name=$1
    local lib_name_stripped=$(basename $lib_name | sed -n -e 's/\(.\+\.so\)\(\.[0-9]\+\)*$/\1/p')
    echo $lib_name_stripped
}

are_mpi_libraries_abi_compatible()
{
    local site_lib=$1
    local site_lib_version_numbers=($(echo $(basename $site_lib) | sed 's/lib.*\.so\(.*\)/\1/' | sed -e 's/^\.\(.*\)/\1/' | sed -e 's/\./ /g'))
    local count_site_version_numbers=${#site_lib_version_numbers[*]}

    local container_lib=$2
    local container_lib_version_numbers=($(echo $(basename $container_lib) | sed 's/lib.*\.so\(.*\)/\1/' | sed -e 's/^\.\(.*\)/\1/' | sed -e 's/\./ /g'))
    local count_container_version_numbers=${#container_lib_version_numbers[*]}

    if [ $count_site_version_numbers -gt $count_container_version_numbers ]; then
        log ERROR "internal error: missing version information about the container MPI shared library $site_lib. \
The library name should contain at least $count_site_version_numbers version numbers."
        exit 1
    fi

    if [ $count_site_version_numbers -ge 1 ]; then
        if [ ${site_lib_version_numbers[0]} -ne ${container_lib_version_numbers[0]} ]; then
            return 1
        fi
    fi

    if [ $count_site_version_numbers -ge 2 ]; then
        if [ ${site_lib_version_numbers[1]} -lt ${container_lib_version_numbers[1]} ]; then
            return 1
        fi
    fi

    return 0
}

get_abi_compatible_site_mpi_library()
{
    local container_lib=$1
    for site_lib in $site_mpi_shared_libraries; do
        local site_lib_id=$(echo $site_lib | cut -d: -f1)
        local site_lib_realpath=$(echo $site_lib | cut -d: -f2)
        if [ $(strip_library_name $site_lib_id) = $(strip_library_name $container_lib) ]; then
            if are_mpi_libraries_abi_compatible $site_lib_realpath $container_lib; then
                echo $site_lib_realpath
                return
            fi
        fi
    done
}

cached_site_mpi_library_ids_stripped()
{
    # if cache is empty let's populate it
    if [ -z "$cached_lib_ids" ]; then
        for lib in $site_mpi_shared_libraries; do
            local lib_id=$(echo $lib | cut -d: -f1)
            local lib_id_stripped=$(strip_library_name $lib_id)
            export cached_lib_ids="$cached_lib_ids $lib_id_stripped"
        done
    fi
    echo "$cached_lib_ids"
}

corresponding_site_mpi_library_exists()
{
    local container_lib_stripped=$(strip_library_name $1)
    local site_libs_stripped=$(cached_site_mpi_library_ids_stripped)
    for site_lib_stripped in $site_libs_stripped; do
        if [ $site_lib_stripped = $container_lib_stripped ]; then
            return 0
        fi
    done
    return 1
}

override_mpi_shared_libraries()
{
    local container_lib_realpaths=$(chroot $container_root_dir bash -c "ldconfig -p | sed -n -e 's/.* => \(.*\)/echo \$(realpath \1)/p' | bash")

    for container_lib_realpath in $container_lib_realpaths; do
        if corresponding_site_mpi_library_exists $container_lib_realpath; then
            local site_mpi_lib=$(get_abi_compatible_site_mpi_library $container_lib_realpath)
            local mount_point=$container_root_dir/$container_lib_realpath
            if [ ! -z $site_mpi_lib ]; then
                mount --bind $site_mpi_lib $mount_point
                exit_if_previous_command_failed "cannot bind mount $site_mpi_lib to $mount_point"
            else
                log ERROR "cannot find a site MPI library which is ABI-compatible with the container library $container_lib_realpath"
                exit 1
            fi
        fi
    done
}

bind_mount_site_mpi_dependencies()
{
    bind_mount_files "$site_mpi_dependency_libraries" "$container_lib_dir"
    bind_mount_files "$site_configuration_files" "$container_root_dir/etc/libibverbs.d"
}

bind_mount_mpi_binaries()
{
    bind_mount_files "$site_binaries" "$container_bin_dir"
}

# make shure that the following function is called at least once
# in this shell (not a subshell) to populate the cache
cached_site_mpi_library_ids_stripped >/dev/null

parse_command_line_arguments $*
check_that_image_contains_required_dependencies
check_that_image_contains_mpi_libraries
override_mpi_shared_libraries
bind_mount_site_mpi_dependencies
bind_mount_mpi_binaries
