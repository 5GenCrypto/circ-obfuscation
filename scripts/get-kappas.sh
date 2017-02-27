#!/usr/bin/env bash

prog=$(readlink -f ../circobf.sh)
circuits=$(readlink -f ../circuits)

run () {
    circuit=$1
    issigma=$2

    if [ x$issigma = x"y" ]; then
        flags="--sigma --symlen 16"
    fi
    name=$(basename $circuit | cut -d'.' -f1)
    mode=$(basename $circuit | cut -d'.' -f2)
    test $mode != "dsl" && test $mode != "c2a" && test $mode != "c2c" && mode=""
    $prog --get-kappa --scheme LIN --verbose $circuit $flags &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lin=$(grep "κ = " /tmp/results.txt | cut -d' ' -f 3)
        lin_=$(grep "* κ" /tmp/results.txt | tr -s ' ' | cut -d' ' -f 3)
    else
        lin="[overflow]"
        lin_="[overflow]"
    fi
    ninputs=$(grep ninputs /tmp/results.txt | cut -d' ' -f 3)
    nconsts=$(grep nconsts /tmp/results.txt | cut -d' ' -f 3)
    noutputs=$(grep noutputs /tmp/results.txt | cut -d' ' -f 3)
    size=$(grep "* size" /tmp/results.txt | cut -d' ' -f 3)
    nmuls=$(grep " *nmuls" /tmp/results.txt | cut -d' ' -f 3)
    depth=$(grep "* depth" /tmp/results.txt | cut -d' ' -f 3)
    degree=$(grep "* degree" /tmp/results.txt | cut -d' ' -f 3)
    $prog --get-kappa --scheme LZ --verbose $circuit $flags &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lz=$(grep "κ =" /tmp/results.txt | cut -d' ' -f 3)
        lz_=$(grep "* κ" /tmp/results.txt | tr -s ' ' | cut -d' ' -f 3)
    else
        lz="[overflow]"
        lz_="[overflow]"
    fi
    echo "$name, $mode, $ninputs, $nconsts, $noutputs, $size, $nmuls, $depth, $degree, $lin_ | $lin, $lz_ | $lz"
}

echo "name, mode, nins, nkey, nouts, size, nmuls, depth, degree, lin.κ, lz.κ"
for circuit in $(ls $circuits/*.acirc); do
    run $circuit n
done
for circuit in $(ls -v $circuits/circuits/*.acirc); do
    run $circuit n
done
for circuit in $(ls -v $circuits/circuits/sigma/*.acirc); do
    run $circuit y
done
# for circuit in $(ls ../circuits/circuits/other/*.acirc); do
#     run $circuit n
# done
