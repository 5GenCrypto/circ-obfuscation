#!/usr/bin/env bash

#abort if any command fails
set -e

dir=$(dirname $(readlink -f "${BASH_SOURCE[0]}"))

builddir=$dir/build
prog=$dir/src/circobf

export LD_LIBRARY_PATH=$builddir/lib

$prog "$@"
