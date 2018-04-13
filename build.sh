#!/usr/bin/env bash

#abort if any command fails
set -e

mkdir -p build/bin build/include build/lib
builddir=$(readlink -f build)
build_cxs=1

usage () {
    cat <<EOF
circ-obfuscation build script

usage: build.sh [options]

Optional arguments:

    -c, --clean    Clean before building
    -d, --debug    Build in debug mode
    --cxs          Build 'cxs'
    -h, --help     Print this information and exit

EOF
}

debug=''
build_cxs=0
while [[ $# -gt 0 ]]; do
    case "${1}" in
        -c|--clean)
            rm -rf build libaesrand clt13 libmmap libacirc libthreadpool circuit-synthesis *.so mio
            shift
            ;;
        -d|--debug)
            debug='-DCMAKE_BUILD_TYPE=Debug'
            shift
            ;;
        --cxs)
            build_cxs=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument '${1}'"
            usage
            exit 1
            ;;
    esac
done

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
        ln -sf "${PWD}"/cxs "${builddir}/bin/cxs"
        ln -sf "${PWD}"/boots "${builddir}/bin/boots"
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

git pull origin dev
build libaesrand        https://github.com/5GenCrypto/libaesrand cmake
build clt13             https://github.com/5GenCrypto/clt13 dev
build libmmap           https://github.com/5GenCrypto/libmmap dev
build libthreadpool     https://github.com/5GenCrypto/libthreadpool dev
build libacirc          https://github.com/amaloz/libacirc dev
if [[ "${build_cxs}" == 1 ]]; then
    build circuit-synthesis https://github.com/spaceships/circuit-synthesis dev
fi

echo
echo Building mio
echo

echo $CFLAGS

rm -rf CMakeCache CMakeFiles
cmake "${debug}" -DCMAKE_C_FLAGS:STRING="${CFLAGS}" -DCMAKE_LIBRARY_PATH:STRING="${builddir}/lib" .
make
