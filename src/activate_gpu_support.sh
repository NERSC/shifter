#!/bin/bash

#here is necessary to set PATH manually because shifter might execute this script
#with an empty environment (after calling the clearenv function)
export PATH=/usr/local/bin:/usr/bin:/bin:/sbin

gpu_ids=
container_mount_point=
nvidia_bin_path=
nvidia_lib_path=
nvidia_lib64_path=

#the NVIDIA compute libraries that will be bind mounted into the container
nvidia_compute_libs="cuda \
                    nvcuvid \
                    nvidia-compiler \
                    nvidia-encode \
                    nvidia-ml \
                    nvidia-fatbinaryloader"

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

    printf "[ GPU SUPPORT ACTIVATION ] =$level= $msg\n" >&2
}

exit_if_previous_command_failed()
{
    if [ $? -ne 0 ]; then
        log ERROR "internal error"
        exit 1
    fi
}

parse_command_line_arguments()
{
    if [ $# -ne 5 ]; then
        log ERROR "internal error: received bad number of command line arguments"
        exit 1
    fi
    
    gpu_ids=$(echo $1 | sed 's/,/ /g')
    container_mount_point=$2
    nvidia_bin_path=$3
    nvidia_lib_path=$4
    nvidia_lib64_path=$5

    if [ ! -d $container_mount_point ]; then
        log ERROR "internal error: received invalid value for 'mount point' command line argument"
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
    local lib_path_container=$container_mount_point/$nvidia_lib_path
    local lib64_path_container=$container_mount_point/$nvidia_lib64_path
    
    mkdir -p $lib_path_container
    exit_if_previous_command_failed
    mkdir -p $lib64_path_container
    exit_if_previous_command_failed

    for lib in $nvidia_compute_libs; do
        local libs_host=$( ldconfig -p | grep "lib${lib}.so" | awk '{print $4}' )
        if [ -z "$libs_host" ]; then
            log WARN "could not find library: $lib"
            continue
        fi
        
        for lib_host in $libs_host; do
            local arch=$( file -L $lib_host | awk '{print $3}' | cut -d- -f1 )
            if [ "$arch" = "32" ]; then
                local lib_container=$lib_path_container/$(basename $lib_host)
            elif [ "$arch" = "64" ]; then
                local lib_container=$lib64_path_container/$(basename $lib_host)
            else
                log ERROR "found/parsed invalid CPU architecture of NVIDIA library"
                exit 1
            fi
            
            touch $lib_container
            mount --bind $lib_host $lib_container
        done
    done
}

add_nvidia_binaries_to_container()
{
    local bins_path_container=$container_mount_point/$nvidia_bin_path

    mkdir -p $bins_path_container
    exit_if_previous_command_failed

    for bin in $nvidia_binaries; do
        local bin_host="$( which $bin )"
        if [ -z $bin_host ]; then
            log WARN "could not find binary: $bin"
            continue
        fi
        local bin_container=$bins_path_container/$bin
        touch $bin_container
        mount --bind $bin_host $bin_container
    done
}

load_nvidia_uvm_if_necessary()
{
    if [ ! -e /dev/nvidia-uvm ]; then
        nvidia-modprobe -u -c=0
    fi
}

parse_command_line_arguments $*
check_prerequisites
add_nvidia_compute_libs_to_container
add_nvidia_binaries_to_container
load_nvidia_uvm_if_necessary
