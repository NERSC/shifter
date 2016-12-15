#!/bin/bash

#here is necessary to set PATH manually because shifter executes
#this script with an empty environment
export PATH=/usr/local/bin:/usr/bin:/bin:/sbin

nvidia_devices=
container_mount_point=
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
    if [ $# -eq 3 ]; then
      nvidia_devices=$(echo $1 | tr , '\n' | sed 's/^/\/dev\/nvidia/')
      container_mount_point=$2
      container_site_resources=$container_mount_point$3
      container_bin_path=$container_site_resources/gpu/bin
      container_lib_path=$container_site_resources/gpu/lib
      container_lib64_path=$container_site_resources/gpu/lib64
    else
        log ERROR "internal error: received bad number of command line arguments"
        exit 1
    fi
}

validate_command_line_arguments()
{
    for device in $nvidia_devices; do
        if [ ! -e $device ]; then
            log ERROR "received bad GPU ID. Cannot find device $device"
        fi
    done

    if [ ! -d $container_mount_point ]; then
        log ERROR "internal error: received invalid \"mount point\". Directory $container_mount_point doesn't exist."
        exit 1
    fi

    if [ ! -d $container_site_resources ]; then
        log ERROR "internal error: received invalid \"site resources\". Directory $container_site_resources doesn't exist."
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
    mkdir -p $container_lib_path
    exit_if_previous_command_failed
    mkdir -p $container_lib64_path
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
                local lib_container=$container_lib_path/$(basename $lib_host)
            elif [ "$arch" = "64" ]; then
                local lib_container=$container_lib64_path/$(basename $lib_host)
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
    mkdir -p $container_bin_path
    exit_if_previous_command_failed

    for bin in $nvidia_binaries; do
        local bin_host="$( which $bin )"
        if [ -z $bin_host ]; then
            log WARN "could not find binary: $bin"
            continue
        fi
        local bin_container=$container_bin_path/$bin
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

get_container_device_file()
{
    local container_device_prefix=$(echo $container_mount_point/dev | sed 's/\//\\\//g')
    echo $1 | sed "s/^/$container_device_prefix\//"
}

get_host_device_file()
{
    echo $1 | sed 's/^/\/dev\//'
}

is_value_not_in_list()
{
    local element=$1
    local list=$2
    for list_element in $list; do
        if [ "$element" = "$list_element" ]; then
            return 1
        fi
    done
    return 0
}

parse_command_line_arguments $*
validate_command_line_arguments
check_prerequisites
add_nvidia_compute_libs_to_container
add_nvidia_binaries_to_container
load_nvidia_uvm_if_necessary
