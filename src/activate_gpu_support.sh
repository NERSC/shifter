#!/bin/bash

#here is necessary to set PATH manually because shifter might execute this script
#with an empty environment (after calling the clearenv function)
export PATH=/usr/local/bin:/usr/bin:/bin:/sbin

#TODO: replace these variables with parameters passed to the script by the shifter executable
container_mount_point=/var/udiMount
nvidia_libs_mount_point=$container_mount_point/gpu-support/lib/nvidia
nvidia_bins_mount_point=$container_mount_point/gpu-support/bin/nvidia

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

__log()
{
    local level="$1"
    local msg="$2"

    printf "[ GPU SUPPORT ACTIVATION ] =$level= $msg\n" >&2
}

check_prerequisites()
{
    local cmds="nvidia-smi nvidia-modprobe"

    for cmd in $cmds; do
        command -v $cmd >/dev/null && continue
        __log ERROR "Command not found: $cmd"
        exit 1
    done
}

add_nvidia_compute_libs_to_container()
{
    mkdir -p $nvidia_libs_mount_point/lib
    mkdir -p $nvidia_libs_mount_point/lib64

    for lib in $nvidia_compute_libs; do
        local lib_paths=$( ldconfig -p | grep "lib${lib}.so" | awk '{print $4}' )
        if [ -z "$lib_paths" ]; then
            __log WARN "could not find library: $lib"
            continue
        fi
        
        for lib_path in $lib_paths; do
            local lib_arch=$( file -L $lib_path | awk '{print $3}' | cut -d- -f1 )
            if [ "$lib_arch" = "32" ]; then
                local lib_mount_point=$nvidia_libs_mount_point/lib/$(basename $lib_path)
            elif [ "$lib_arch" = "64" ]; then
                local lib_mount_point=$nvidia_libs_mount_point/lib64/$(basename $lib_path)      
            else
                __log ERROR "found/parsed invalid CPU architecture of NVIDIA library"
                exit 1
            fi
            
            touch $lib_mount_point
            mount --bind $lib_path $lib_mount_point
        done
    done
}

add_nvidia_binaries_to_container()
{
    mkdir -p $nvidia_bins_mount_point

    for bin in $nvidia_binaries; do
        local bin_path="$( which $bin )"
        if [ -z $bin_path ]; then
            __log WARN "could not find binary: $bin"
            continue
        fi
        local bin_mount_point=$nvidia_bins_mount_point/$bin
        touch $bin_mount_point
        mount --bind $bin_path $bin_mount_point
    done
}

load_nvidia_uvm_if_necessary()
{
    if [ ! -e /dev/nvidia-uvm ]; then
        nvidia-modprobe -u -c=0
    fi
}

check_prerequisites
add_nvidia_compute_libs_to_container
add_nvidia_binaries_to_container
load_nvidia_uvm_if_necessary
