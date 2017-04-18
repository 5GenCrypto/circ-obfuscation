#!/usr/bin/env bash
#
# Prints info about circuit and outputs as CSV
#

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
prog=$(readlink -f "$dir/../circobf.sh")
circuits=$(readlink -f "$dir/../circuits")

get () {
    grep "$1" /tmp/results.txt | tr -s ' ' | awk -F' ' '{print $NF}'
}

run () {
    circuit=$1
    issigma=$2

    if [[ $issigma == y ]]; then
        flags="--sigma --symlen 16"
    fi
    name=$(basename "$circuit" | cut -d'.' -f1)
    mode=$(basename "$circuit" | cut -d'.' -f2)
    if [[ $mode != c2a && $mode != c2v && $mode != dsl && $mode != opt ]]; then
        mode=""
    fi
    $prog --get-kappa --scheme LIN --verbose $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lin1=$(get "κ = ")
    else
        lin1="[overflow]"
    fi
    ninputs=$(get "ninputs")
    nconsts=$(get "nconsts")
    noutputs=$(get "noutputs")
    ngates=$(get "* ngates")
    nmuls=$(get " *nmuls")
    depth=$(get "* depth")
    degree=$(get "* degree")
    $prog --get-kappa --scheme LIN --verbose --smart $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lin2=$(get "κ = ")
    else
        lin2="[overflow]"
    fi
    $prog --get-kappa --scheme LZ --verbose $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lz1=$(get "κ = ")
    else
        lz1="[overflow]"
    fi
    $prog --get-kappa --scheme LZ --verbose --smart $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lz2=$(get "κ = ")
    else
        lz2="[overflow]"
    fi
    echo "$name, $mode, $ninputs, $nconsts, $noutputs, $ngates, $nmuls, $depth, $degree, $lin1 | $lin2, $lz1 | $lz2"
    rm -f $circuit.obf
}

echo "name, mode, nins, nkey, nouts, ngates, nmuls, depth, degree, lin.κ, lz.κ"
if [[ $1 != "" ]]; then
    run "$1" n
else
    for circuit in $(ls -v "$circuits"/*.acirc); do
        run "$circuit" n
    done
    for circuit in $(ls -v "$circuits"/sigma/*.acirc); do
        run "$circuit" y
    done
fi
