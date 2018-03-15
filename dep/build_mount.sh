#!/bin/bash
set -e

unset CFLAGS
unset CPPFLAGS

origdir=$( pwd )
mkdir -p build_mount
cd build_mount
builddir=$( pwd )

if [[ -n "$DEPTAR_DIR" && -e "$DEPTAR_DIR/util-linux-2.26.2.tar.gz" ]]; then
    cp "$DEPTAR_DIR/util-linux-2.26.2.tar.gz" .
fi

if [[ ! -e "util-linux-2.26.2.tar.gz" ]]; then
    curl -k -o "util-linux-2.26.2.tar.gz" "https://mirrors.edge.kernel.org/pub/linux/utils/util-linux/v2.26/util-linux-2.26.2.tar.gz"
fi

cd "${builddir}"
mkdir -p util-linux
tar xf "util-linux-2.26.2.tar.gz" -C util-linux --strip-components=1
cd util-linux
CC=gcc ./configure --enable-static --disable-shared
CC=gcc make mount
cp -p mount "${origdir}/"
