#!/bin/bash
set -e

CPPUTEST_VERSION=3.7dev

baseDir=$(pwd)
mkdir -p cpputest
cd cpputest

if [[ ! -e "cpputest-${CPPUTEST_VERSION}.tar.gz" && -n "$DEPTAR_DIR" && -e "$DEPTAR_DIR/cpputest-${CPPUTEST_VERSION}.tar.gz" ]]; then
    cp "$DEPTAR_DIR/cpputest-${CPPUTEST_VERSION}.tar.gz" .
fi
if [[ ! -e "cpputest-${CPPUTEST_VERSION}.tar.gz" ]]; then
    curl -L -o "cpputest-${CPPUTEST_VERSION}.tar.gz" "https://github.com/cpputest/cpputest.github.io/blob/master/releases/cpputest-${CPPUTEST_VERSION}.tar.gz?raw=true"
fi

mkdir -p cpputest_src
tar xf "cpputest-${CPPUTEST_VERSION}.tar.gz" -C cpputest_src --strip-components=1
cd cpputest_src
# make sure we have the last config.guess available, this helps when building on openpower
curl 'http://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=HEAD' -o config.guess
./configure --prefix="${baseDir}/cpputest"
make
make install
cd ..
rm -rf cpputest_src
