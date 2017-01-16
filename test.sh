#!/usr/bin/env bash

set -e

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
    ./run.sh --all --mmap $mmap --scheme $scheme --debug $debug $circuit
}

run_rachel () {
    circuit=$1
    scheme=$2
    mmap=$3
    debug=$4

    echo
    echo \*\*\*
    echo \*\*\*
    echo \*\*\* $circuit $scheme $mmap $debug RACHEL
    echo \*\*\*
    echo \*\*\*
    echo
    ./run.sh --all --mmap $mmap --scheme $scheme --debug $debug $circuit --rachel --symlen 16
}

for circuit in $(ls circuits/*.acirc); do
    echo $circuit
    run $circuit LIN DUMMY ERROR
    run $circuit LZ  DUMMY ERROR
done

for circuit in $(ls circuits/all-circuits/*.acirc); do
    echo $circuit
    run $circuit LIN DUMMY ERROR
    run $circuit LZ  DUMMY ERROR
done

for circuit in $(ls circuits/all-circuits/ggm/*.acirc); do
    echo $circuit
    run $circuit LIN DUMMY ERROR
    run $circuit LZ  DUMMY ERROR
done

for circuit in $(ls circuits/all-circuits/ggm_rachel/*.acirc); do
    echo $circuit
    run_rachel $circuit LIN DUMMY ERROR
    run_rachel $circuit LZ  DUMMY ERROR
done
