#!/bin/bash
set -e

CPPUTEST_VERSION=3.8

baseDir=$(pwd)
mkdir -p cpputest
cd cpputest

if [[ ! -e "cpputest-${CPPUTEST_VERSION}.tar.gz" && -n "$DEPTAR_DIR" && -e "$DEPTAR_DIR/cpputest-${CPPUTEST_VERSION}.tar.gz" ]]; then
    cp "$DEPTAR_DIR/cpputest-${CPPUTEST_VERSION}.tar.gz" .
fi
if [[ ! -e "cpputest-${CPPUTEST_VERSION}.tar.gz" ]]; then
    curl -L -k -o "cpputest-${CPPUTEST_VERSION}.tar.gz" "https://github.com/cpputest/cpputest/releases/download/v${CPPUTEST_VERSION}/cpputest-${CPPUTEST_VERSION}.tar.gz"
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
