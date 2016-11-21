#!/bin/bash

cp imagegw/test.json.example test.json
rm imagegw/test/sha256sum
./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc/shifter --disable-staticsshd --without-slurm
MAKEFLAGS="-j$num_of_processors" make
