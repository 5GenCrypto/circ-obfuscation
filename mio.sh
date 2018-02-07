#!/usr/bin/env bash

DIR=$(dirname $(readlink -f $0))

export PATH=$DIR/build/bin:$PATH
export LD_LIBRARY_PATH=$DIR/build/lib

$DIR/mio "$@"
