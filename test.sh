#!/usr/bin/env bash

# set -e

run () {
    circuit=$1
    scheme=$2
    mmap=$3
    debug=$4

    echo
    echo \*\*\*
    echo \*\*\*
    echo \*\*\* $circuit $scheme $mmap $debug
    echo \*\*\*
    echo \*\*\*
    echo
    ./run.sh --mmap $mmap --scheme $scheme --debug $debug --verbose circuits/$circuit
}

for circuit in $(ls circuits); do
    run $circuit ZIM DUMMY ERROR
    run $circuit AB  DUMMY ERROR
    run $circuit LIN DUMMY ERROR
done
