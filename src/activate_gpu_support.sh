#!/bin/bash

#here is necessary to set PATH manually because shifter executes
#this script with an empty environment
export PATH=/usr/local/bin:/usr/bin:/bin:/sbin

cuda_devices=
container_root_dir=
container_site_resources=
is_verbose_active=
container_bin_path=
container_lib_path=
container_lib64_path=

#the NVIDIA compute libraries that will be bind mounted into the container
nvidia_compute_libs="cuda \
                    nvidia-compiler \
                    nvidia-ptxjitcompiler \
                    nvidia-encode \
                    nvidia-ml \
                    nvidia-fatbinaryloader \
                    nvidia-opencl"

#the NVIDIA binaries that will be bind mounted into the container
nvidia_binaries="nvidia-cuda-mps-control \
               nvidia-cuda-mps-server \
               nvidia-debugdump \
               nvidia-persistenced \
               nvidia-smi"

log()
{
    local level="$1"
    local msg="$2"
    if [ $is_verbose_active = false ]; then
        if [ $level = DEBUG -o $level = INFO ]; then
            return
        fi
    fi
    printf "[ GPU SUPPORT ] =$level= $msg\n" >&2
}

exit_if_previous_command_failed()
{
    if [ $? -ne 0 ]; then
        log ERROR "internal error"
        exit 1
    fi
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

parse_command_line_arguments()
{
    if [ ! $# -eq 4 ]; then
        log ERROR "Internal error: received bad number of command line arguments"
        exit 1
    fi

    cuda_devices=$1
    container_root_dir=$2
    container_site_resources=$3
    container_bin_path=$container_site_resources/gpu/bin
    container_lib_path=$container_site_resources/gpu/lib
    container_lib64_path=$container_site_resources/gpu/lib64

    local verbose=$4
    if [ $verbose = "verbose-on" ]; then
        is_verbose_active=true
    elif [ $verbose = "verbose-off" ]; then
        is_verbose_active=false
    else
        log ERROR "Internal error: received bad \"verbose\" parameter"
    fi
}

validate_command_line_arguments()
{
    if [ ! -d $container_root_dir ]; then
        log ERROR "internal error: received invalid \"container's root directory\". Directory $container_root_dir doesn't exist."
        exit 1
    fi

    if [ ! -d $container_root_dir$container_site_resources ]; then
        log ERROR "internal error: received invalid \"site resources\". Directory $container_root_dir$container_site_resources doesn't exist."
        exit 1
    fi
}

check_prerequisites()
{
    local cmds="nvidia-smi nvidia-modprobe"
    for cmd in $cmds; do
        command -v $cmd >/dev/null && continue
        log ERROR "Command not found: $cmd"
        exit 1
    done
}

add_nvidia_compute_libs_to_container()
{
    for lib in $nvidia_compute_libs; do
        local libs_host=$( ldconfig -p | grep "lib${lib}.so" | awk '{print $4}' )
        if [ -z "$libs_host" ]; then
            log WARNING "could not find library: $lib"
            continue
        fi

        for lib_host in $libs_host; do
            local arch=$( file -L $lib_host | awk '{print $3}' | cut -d- -f1 )
            if [ "$arch" = "32" ]; then
                local lib_container=$container_lib_path/$(basename $lib_host)
            elif [ "$arch" = "64" ]; then
                local lib_container=$container_lib64_path/$(basename $lib_host)
            else
                log ERROR "found/parsed invalid CPU architecture of NVIDIA library"
                exit 1
            fi
            bind_mount_file_into_container $lib_host $lib_container
        done
    done
}

add_nvidia_binaries_to_container()
{
    for bin in $nvidia_binaries; do
        local bin_host="$( which $bin )"
        if [ -z $bin_host ]; then
            log WARNING "could not find binary: $bin"
            continue
        fi
        local bin_container=$container_bin_path/$bin
        bind_mount_file_into_container $bin_host $bin_container
    done
}

load_nvidia_uvm_if_necessary()
{
    if [ ! -e /dev/nvidia-uvm ]; then
        nvidia-modprobe -u -c=0
    fi
}

parse_command_line_arguments $*
validate_command_line_arguments
log INFO "activating support for CUDA devices $cuda_devices."
check_prerequisites
add_nvidia_compute_libs_to_container
add_nvidia_binaries_to_container
load_nvidia_uvm_if_necessary
