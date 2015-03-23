#!/bin/bash

mkdir -p $PREFIX/etc_files
getent passwd > $PREFIX/etc_files/docker_passwd
getent group > $PREFIX/etc_files/docker_group
cp /etc/resolv.conf $PREFIX/etc_files/resolv.conf
cp /etc/clustername $PREFIX/etc_files/clustername
