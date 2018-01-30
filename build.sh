#!/usr/bin/env bash

#abort if any command fails
set -e

mkdir -p build/bin build/include build/lib
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

if [[ "$1" == "" ]]; then
    debug=''
elif [[ "$1" == "debug" ]]; then
    debug='-DCMAKE_BUILD_TYPE=Debug'
elif [[ "$1" == "clean" ]]; then
    rm -rf build libaesrand clt13 libmmap libacirc libthreadpool circuit-synthesis *.so mio
    exit 0
elif [[ "$1" == "help" ]]; then
    usage
    exit 0
else
    echo "error: unknown command '$1'"
    usage
    exit 1
fi

export CFLAGS=-I$builddir/include
export LDFLAGS=-L$builddir/lib

build () {
    path=$1
    url=$2
    branch=$3
    flags=$debug
    if [[ "${path}" == "libaesrand" ]]; then
        flags+=" -DHAVE_FLINT=OFF -DHAVE_MPFR=OFF"
    fi
    if [[ "${path}" == "libmmap" ]]; then
        flags+=" -DHAVE_GGHLITE=OFF"
    fi

    echo
    echo "building $path ($url $branch) [flags=$flags]"
    echo

    if [ ! -d $path ]; then
        git clone $url $path -b $branch;
    else
        pushd $path; git pull origin $branch; popd
    fi
    if [[ $path == "circuit-synthesis" ]]; then
        pushd $path
        ./build-app.sh
        cp cxs boots "${builddir}/bin"
        popd
    else
        pushd $path
        cmake -DCMAKE_INSTALL_PREFIX="${builddir}" $flags .
        make
        make install
        popd
    fi
}

echo builddir = $builddir

build libaesrand        https://github.com/5GenCrypto/libaesrand cmake
build clt13             https://github.com/5GenCrypto/clt13 dev
build libmmap           https://github.com/5GenCrypto/libmmap dev
build libthreadpool     https://github.com/5GenCrypto/libthreadpool dev
build libacirc          https://github.com/amaloz/libacirc dev
build circuit-synthesis https://github.com/spaceships/circuit-synthesis dev

echo
echo Building mio
echo

echo $CFLAGS

rm -rf CMakeCache CMakeFiles
cmake "${debug}" -DCMAKE_C_FLAGS:STRING="${CFLAGS}" -DCMAKE_LIBRARY_PATH:STRING="${builddir}/lib" .
make
