#!/usr/bin/env bash

set -e

prog=$(readlink -f run.sh)

run () {
    circuit=$1
    israchel=$2

    if [ $(head -1 $circuit | cut -d' ' -f 1) != ":test" ]; then
        return
    fi

    if [ $israchel = "y" ]; then
        flags="--rachel --symlen 16"
    fi
    name=$(basename $circuit | cut -d'.' -f1)
    mode=$(basename $circuit | cut -d'.' -f2)
    test $mode != "dsl" && test $mode != "c2a" && test $mode != "c2c" && mode=""
    $prog --get-kappa --scheme LIN --verbose $circuit $flags &>/tmp/results.txt
    ninputs=$(grep ninputs /tmp/results.txt | cut -d' ' -f 3)
    nconsts=$(grep nconsts /tmp/results.txt | cut -d' ' -f 3)
    noutputs=$(grep noutputs /tmp/results.txt | cut -d' ' -f 3)
    size=$(grep "* size" /tmp/results.txt | cut -d' ' -f 3)
    nmuls=$(grep " *nmuls" /tmp/results.txt | cut -d' ' -f 3)
    depth=$(grep "* depth" /tmp/results.txt | cut -d' ' -f 3)
    degree=$(grep "* degree" /tmp/results.txt | cut -d' ' -f 3)
    lin=$(grep "κ = " /tmp/results.txt | cut -d' ' -f 3)
    $prog --get-kappa --scheme LZ --verbose $circuit $flags &>/tmp/results.txt
    lz=$(grep "κ =" /tmp/results.txt | cut -d' ' -f 3)
    echo "$name, $mode, $ninputs, $nconsts, $noutputs, $size, $nmuls, $depth, $degree, $lin, $lz"
}

echo "name, mode, nins, nkey, nouts, size, nmuls, depth, degree, lin.κ, lz.κ"
for circuit in $(ls circuits/*.acirc); do
    run $circuit n
done
for circuit in $(ls circuits/circuits/*.acirc); do
    run $circuit n
done
for circuit in $(ls circuits/all-circuits/rachel/*.acirc); do
    run $circuit y
done
