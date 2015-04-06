#!/bin/bash

mkdir -p $PREFIX/etc_files
getent passwd > $PREFIX/etc_files/udi_passwd
getent group | awk '{ if (length($0) < 512) { print $0; } }' > $PREFIX/etc_files/udi_group
cp /etc/resolv.conf $PREFIX/etc_files/resolv.conf
cp /etc/clustername $PREFIX/etc_files/clustername
cp -rp etc_static/* $PREFIX/etc_files
