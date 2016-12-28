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
    ./run.sh --all --mmap $mmap --scheme $scheme --debug $debug --verbose $circuit
}

# for circuit in $(ls circuits/*.acirc); do
#     run $circuit ZIM DUMMY ERROR
#     run $circuit AB  DUMMY ERROR
#     run $circuit LIN DUMMY ERROR
#     run $circuit ZIM CLT ERROR
#     run $circuit AB  CLT ERROR
#     run $circuit LIN CLT ERROR
# done

for circuit in $(ls circuits/ggm/*.acirc); do
    run $circuit ZIM DUMMY ERROR
    run $circuit LIN DUMMY ERROR
done
