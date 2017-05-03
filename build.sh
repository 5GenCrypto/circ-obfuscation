#!/usr/bin/env bash

#abort if any command fails
set -e

mkdir -p build/autoconf
builddir=$(readlink -f build)

usage () {
    echo "circ-obfuscation build script"
    echo ""
    echo "Commands:"
    echo "  <default>   Build everything"
    echo "  debug       Build in debug mode"
    echo "  clean       Remove build"
    echo "  help        Print this info and exit"
}

if [ x"$1" == x"" ]; then
    debug=''
elif [ x"$1" == x"debug" ]; then
    debug='--enable-debug'
elif [ x"$1" == x"clean" ]; then
    rm -rf build libaesrand clt13 libmmap libacirc libthreadpool
    exit 0
elif [ x"$1" == x"help" ]; then
    usage
    exit 0
else
    echo "error: unknown command '$1'"
    usage
    exit 1
fi

export CPPFLAGS=-I$builddir/include
export CFLAGS=-I$builddir/include
export LDFLAGS=-L$builddir/lib

build () {
    path=$1
    url=$2
    branch=$3
    flags=$debug
    if [ $path = "libmmap" ]; then
        flags+=" --without-gghlite"
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
        make check
        make install
    popd
}

echo builddir = $builddir

# build libaesrand    https://github.com/5GenCrypto/libaesrand master
# build clt13         https://github.com/5GenCrypto/clt13 master
# build libmmap       https://github.com/5GenCrypto/libmmap master
# build libacirc      https://github.com/5GenCrypto/libacirc master
# build libthreadpool https://github.com/5GenCrypto/libthreadpool master

autoreconf -i
./configure --prefix=$builddir $debug
make
