#!/bin/bash
set -e

CPPUTEST_VERSION=3.7dev

baseDir=$(pwd)
mkdir -p cpputest
cd cpputest

if [[ ! -e "cpputest-${CPPUTEST_VERSION}.tar.gz" ]]; then
    curl -L -o "cpputest-${CPPUTEST_VERSION}.tar.gz" "https://github.com/cpputest/cpputest.github.io/blob/master/releases/cpputest-3.7dev.tar.gz?raw=true"
fi

mkdir -p cpputest_src
tar xf "cpputest-${CPPUTEST_VERSION}.tar.gz" -C cpputest_src --strip-components=1
cd cpputest_src
./configure --prefix="${baseDir}/cpputest"
make
make install
cd ..
rm -rf cpputest_src
