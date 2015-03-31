#!/bin/bash
set -e

PREFIX=/opt/proteus
MUSL_VERSION=1.1.8
LIBRESSL_VERSION=2.1.6
ZLIB_VERSION=1.2.8
OPENSSH_VERSION=6.8p1

BASEDIR=$( pwd )
#mkdir -p musl
#tar xf "musl-${MUSL_VERSION}.tar.gz" -C musl --strip-components=1
#cd musl
#./configure "--prefix=${PREFIX}" --enable-static --disable-shared
#make
#make install
#cd "${BASEDIR}"

dirs="linux asm asm-generic"
for dir in $dirs; do
    cp -rp "/usr/include/$dir" "${PREFIX}/include/"
done

mkdir -p libressl
tar xf "libressl-${LIBRESSL_VERSION}.tar.gz" -C libressl --strip-components=1
cd libressl
CC="${PREFIX}/bin/musl-gcc" ./configure "--prefix=${PREFIX}" --enable-static --disable-shared
make
make install

cd "${BASEDIR}"
mkdir -p zlib
tar xf "zlib-${ZLIB_VERSION}.tar.gz" -C zlib --strip-components=1
cd zlib
CC="${PREFIX}/bin/musl-gcc" ./configure "--prefix=${PREFIX}"
make
make install

cd "${BASEDIR}"
mkdir -p openssh
tar xf "openssh-${OPENSSH_VERSION}.tar.gz" -C openssh --strip-components=1
cd openssh
CC="${PREFIX}/bin/musl-gcc" ./configure --without-pam "--with-ssl-dir=${PREFIX}" --without-ssh1 --enable-static --disable-shared "--with-zlib=${PREFIX}" "--prefix=${PREFIX}"
make
make install
cd "${BASEDIR}"
