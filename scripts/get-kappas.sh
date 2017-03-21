#!/usr/bin/env bash
#
# Prints info about circuit and outputs as CSV
#

prog=$(readlink -f ../circobf.sh)
circuits=$(readlink -f ../circuits)

get () {
    grep "$1" /tmp/results.txt | tr -s ' ' | awk -F' ' '{print $NF}'
}

run () {
    circuit=$1
    issigma=$2

    if [ x$issigma = x"y" ]; then
        flags="--sigma --symlen 16"
    fi
    name=$(basename $circuit | cut -d'.' -f1)
    mode=$(basename $circuit | cut -d'.' -f2)
    test $mode != "dsl" && test $mode != "c2a" && test $mode != "c2v" && mode=""
    $prog --get-kappa --scheme LIN --verbose $circuit $flags &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lin=$(get "κ = ")
    else
        lin="[overflow]"
    fi
    ninputs=$(get "ninputs")
    nconsts=$(get "nconsts")
    noutputs=$(get "noutputs")
    ngates=$(get "* ngates")
    nmuls=$(get " *nmuls")
    depth=$(get "* depth")
    degree=$(get "* degree")
    $prog --get-kappa --scheme LZ --verbose $circuit $flags &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lz=$(get "κ = ")
    else
        lz="[overflow]"
    fi
    echo "$name, $mode, $ninputs, $nconsts, $noutputs, $ngates, $nmuls, $depth, $degree, $lin, $lz"
}

echo "name, mode, nins, nkey, nouts, ngates, nmuls, depth, degree, lin.κ, lz.κ"
if [ x$1 != x ]; then
    run $1 n
else
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
fi
