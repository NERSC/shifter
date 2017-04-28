#!/bin/bash

#here is necessary to set PATH manually because shifter executes
#this script with an empty environment
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin

site_mpi_shared_libraries=
site_mpi_dependency_libraries=

container_root_dir=
container_mpi_lib_dir=
container_mpi_bin_dir=
is_verbose_active=

log()
{
    local level="$1"
    local msg="$2"
    if [ $is_verbose_active = false ]; then
        if [ $level = DEBUG -o $level = INFO ]; then
            return
        fi
    fi
    printf "[ MPI SUPPORT ] =$level= $msg\n" >&2
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
    if [ ! $# -eq 5 ]; then
        log ERROR "Internal error: received bad number of command line arguments"
        exit 1
    fi

    container_root_dir=$1
    if [ ! -d $container_root_dir ]; then
        log ERROR "Internal error: received bad container root directory ($container_root_dir)"
        exit 1
    fi

    local site_resources=$2
    if [ ! -d $container_root_dir/$site_resources ]; then
        log ERROR "Internal error: received unexisting site-resources folder ($site_resources)"
        exit 1
    fi
    container_mpi_lib_dir=$site_resources/mpi/lib
    container_mpi_bin_dir=$site_resources/mpi/bin

    site_mpi_shared_libraries=$(echo $3 | tr ';' ' ')
    site_mpi_dependency_libraries=$(echo $4 | tr ';' ' ')

    local verbose=$5
    if [ $verbose = "verbose-on" ]; then
        is_verbose_active=true
    elif [ $verbose = "verbose-off" ]; then
        is_verbose_active=false
    else
        log ERROR "Internal error: received bad 'verbose' parameter"
        exit 1
    fi
}

check_that_image_contains_required_dependencies()
{
    # we need the container image to provide these command line tools
    command_line_tools="sed readlink"
    for command_line_tool in $command_line_tools; do
        if [ -z $(chroot $container_root_dir bash -c "which $command_line_tool") ]; then
            log ERROR "Missing dependency: make sure that the container image contains the program \"$command_line_tool\""
            exit 1
        fi
    done
}

bind_mount_file_into_container()
{
    local target=$1
    local container_mount_point=$2
    local mount_point=$container_root_dir$container_mount_point
    local mount_dir=$(dirname $mount_point)

    # create a mount point if necessary
    if [ ! -e $mount_point ]; then
        mkdir -p $mount_dir
        exit_if_previous_command_failed "Cannot mkdir -p $mount_dir"
        touch $mount_point
        exit_if_previous_command_failed "Cannot touch $mount_point"
    fi

    log INFO "Bind mounting site's $target to container's $container_mount_point"
    mount --bind $target $mount_point
    exit_if_previous_command_failed "Cannot mount --bind $target $mount_point"
}

bind_mount_files_into_given_folder_of_container()
{
    local targets=$1
    local container_mount_dir=$2

    for target in $targets; do
        local container_mount_point=$container_mount_dir/$(basename $target)
        bind_mount_file_into_container $target $container_mount_point
    done
}

# drop parent folders and trailing version numbers from the given library name
strip_library_name()
{
    local lib_name=$1
    local lib_name_stripped=$(basename $lib_name | sed -n -e 's/\(.\+\.so\)\(\..\+\)*$/\1/p')
    echo $lib_name_stripped
}

# Returns a list containing the major and minor version strings of the given library name.
# If the library doesn't contain a version string, the version number is assumed to be zero
# and a string containing zero is placed in the list to be returned.
get_mpi_library_major_and_minor_version_numbers()
{
    local library=$1
    local version_strings=($(echo $(basename $library) | sed 's/lib.\+\.so\(.*\)/\1/' | sed -e 's/^\.\(.*\)/\1/' | sed -e 's/\./ /g'))
    local number_of_version_strings=${#version_strings[*]}

    if [ $number_of_version_strings -eq 0 ]; then
        local major_number=0
        local minor_number=0
    elif [ $number_of_version_strings -eq 1 ]; then
        local major_number=${version_strings[0]}
        local minor_number=0
    else
        local major_number=${version_strings[0]}
        local minor_number=${version_strings[1]}
    fi

    is_number_regexp='^[0-9]+$'
    if ! [[ $major_number =~ $is_number_regexp ]]; then
        log ERROR "Internal error: major version string of MPI library $library is not a number"
        exit 1
    fi
    if ! [[ $minor_number =~ $is_number_regexp ]]; then
        log ERROR "Internal error: minor version string of MPI library $library is not a number"
        exit 1
    fi

    echo "$major_number $minor_number"
}

are_mpi_libraries_abi_compatible()
{
    local site_lib=$1
    local site_lib_version_numbers=($(get_mpi_library_major_and_minor_version_numbers $site_lib))

    local container_lib=$2
    local container_lib_version_numbers=($(get_mpi_library_major_and_minor_version_numbers $container_lib))

    if [ ${site_lib_version_numbers[0]} -ne ${container_lib_version_numbers[0]} ]; then
        return 1
    fi

    if [ ${site_lib_version_numbers[1]} -lt ${container_lib_version_numbers[1]} ]; then
        return 1
    fi

    return 0
}

get_abi_compatible_site_mpi_library()
{
    local container_lib=$1
    for site_lib_realpath in $site_mpi_shared_libraries; do
        local site_lib_id=$(strip_library_name $site_lib_realpath)
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
            local lib_id_stripped=$(strip_library_name $lib)
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

override_mpi_shared_libraries_of_container()
{
    log INFO "Scanning container's ld.so.cache for MPI libraries"
    local container_lib_realpaths=$(chroot $container_root_dir bash -c "ldconfig -p | sed -n -e 's/.* => \(.*\)/echo \$(readlink -e \1)/p' | bash")
    local container_lib_realpaths_mounted_so_far=
    local container_has_mpi_libraries=false

    for container_lib_realpath in $container_lib_realpaths; do
        if corresponding_site_mpi_library_exists $container_lib_realpath; then
            container_has_mpi_libraries=true
            local site_mpi_lib=$(get_abi_compatible_site_mpi_library $container_lib_realpath)
            # skip library if has already been bind mounted
            if [[ ! $container_lib_realpaths_mounted_so_far =~ $container_lib_realpath ]]; then
                log INFO "Found $container_lib_realpath in container"
                if [ ! -z $site_mpi_lib ]; then
                    container_lib_realpaths_mounted_so_far="$container_lib_realpaths_mounted_so_far $container_lib_realpath"
                    bind_mount_file_into_container $site_mpi_lib $container_lib_realpath
                else
                    log ERROR "Cannot find a site MPI library which is ABI-compatible with the container library $container_lib_realpath"
                    exit 1
                fi
            fi
        fi
    done

    if [ $container_has_mpi_libraries = false ]; then
        log ERROR "No MPI libraries found in the container.
The container should be configured to access the MPI libraries through the dynamic linker.
Hint: run 'ldconfig' inside the container to configure the dynamic linker
and run 'ldconfig -p' to check the configuration."
        exit 1
    fi
}

# make sure that the following function is called at least once
# in this shell (not a subshell) to populate the cache
cached_site_mpi_library_ids_stripped >/dev/null

parse_command_line_arguments $*
check_that_image_contains_required_dependencies
override_mpi_shared_libraries_of_container
bind_mount_files_into_given_folder_of_container "$site_mpi_dependency_libraries" "$container_mpi_lib_dir"

