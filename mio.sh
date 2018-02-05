#!/usr/bin/env bash

export PATH=$PWD/build/bin:$PATH
export LD_LIBRARY_PATH=$PWD/build/lib

./mio "$@"
