#!/usr/bin/env bash

#abort if any command fails
set -e

mkdir -p build
builddir=$(readlink -f build)

export CPPFLAGS=-I$builddir/include
export CFLAGS=-I$builddir/include
export LDFLAGS=-L$builddir/lib

build () {
    echo building $1
    path=$1
    url=$2
    if [ ! -d $path ]; then
        git clone $url $path;
    else
        cd $path; git pull origin master; cd ..;
    fi
    cd $1
        mkdir -p build/autoconf
        autoreconf -i
        ./configure --prefix=$builddir --enable-debug
        make
        make install
    cd ..;  
    echo
}

echo
echo builddir = $builddir
echo

build libaesrand    git@github.com:5GenCrypto/libaesrand.git
build clt13         git@github.com:5GenCrypto/clt13.git
build libacirc      git@github.com:spaceships/libacirc.git
# build libthreadpool git@github.com:spaceships/libthreadpool.git
