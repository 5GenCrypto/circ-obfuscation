#!/usr/bin/env bash

#abort if any command fails
set -e

dir=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)

builddir=$dir/build
prog=$dir/src/circobf

export LD_LIBRARY_PATH=$builddir/lib

$prog "$@"
