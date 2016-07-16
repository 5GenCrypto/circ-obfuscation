#!/usr/bin/env bash

#abort if any command fails
set -e

echo "libaesrand"
    path=libaesrand
    url=https://github.com/5GenCrypto/libaesrand.git
    if [ ! -d $path ]; then
        git clone $url;
    else
        cd $path; git pull origin master; cd ..;
    fi

echo "clt13"
    path=clt13
    url=https://github.com/5GenCrypto/clt13.git
    if [ ! -d $path ]; then
        git clone $url;
    else
        cd $path; git pull origin master; cd ..;
    fi

echo "libacirc"
    path=libacirc
    url=https://github.com/spaceships/libacirc.git
    if [ ! -d $path ]; then
        git clone $url;
    else
        cd $path; git pull origin master; cd ..;
    fi

mkdir -p build
builddir=$(readlink -f build)

export CPPFLAGS=-I$builddir/include
export CFLAGS=-I$builddir/include
export LDFLAGS=-L$builddir/lib

echo
echo builddir = $builddir
echo

echo building libaesrand
cd libaesrand
    autoreconf -i
    ./configure --prefix=$builddir
    make
    make install
cd ..;  echo

echo building clt13
cd clt13
    mkdir -p build/autoconf
    autoreconf -i
    ./configure --prefix=$builddir
    make
    make install
cd ..; echo

echo building libacirc
cd libacirc
    mkdir -p build/autoconf
    autoreconf -i
    ./configure --prefix=$builddir
    make
    make install
cd ..; echo
