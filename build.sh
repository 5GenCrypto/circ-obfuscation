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
    path=$1
    url=$2
    branch=$3
    flags=$debug
    if [ $path = "libmmap" ]; then
        flags+=" --disable-gghlite"
    fi

    echo
    echo "building $path ($url $branch) [flags=$flags]"
    echo

    if [ ! -d $path ]; then
        git clone $url $path;
    fi
    pushd $path
        git pull origin $branch
        mkdir -p build/autoconf
        autoreconf -i
        ./configure --prefix=$builddir $flags
        make
        make install
    popd
}

echo
echo builddir = $builddir
echo

build libaesrand    git@github.com:5GenCrypto/libaesrand master
build clt13         git@github.com:5GenCrypto/clt13 master
build libmmap       git@github.com:5GenCrypto/libmmap master
build libacirc      git@github.com:5GenCrypto/libacirc master
build libthreadpool git@github.com:5GenCrypto/libthreadpool master

autoreconf -i
./configure --prefix=$builddir $debug
make
