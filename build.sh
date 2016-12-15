#!/usr/bin/env bash

#abort if any command fails
set -e

mkdir -p build/autoconf
builddir=$(readlink -f build)
debug='--enable-debug'

export CPPFLAGS=-I$builddir/include
export CFLAGS=-I$builddir/include
export LDFLAGS=-L$builddir/lib

build () {
    echo building $1
    path=$1
    url=$2
    branch=$3
    if [ ! -d $path ]; then
        git clone $url $path;
    fi
    pushd $path
        git pull origin $branch
        mkdir -p build/autoconf
        autoreconf -i
        ./configure --prefix=$builddir $debug
        make
        make install
    popd
}

echo
echo builddir = $builddir
echo

build libaesrand    https://github.com/5GenCrypto/libaesrand master
build clt13         https://github.com/5GenCrypto/clt13 master
build gghlite       https://github.com/5GenCrypto/gghlite-flint master
build libmmap       https://github.com/5GenCrypto/libmmap master
build libacirc      https://github.com/amaloz/libacirc master
build libthreadpool https://github.com/spaceships/libthreadpool master

autoreconf -i
./configure --prefix=$builddir $debug
make
