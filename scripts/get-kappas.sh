#!/usr/bin/env bash
#
# Prints info about circuit and outputs as CSV
#

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
prog=$(readlink -f "${dir}/../mio")
circuits=$(readlink -f "${dir}/../circuits")

get () {
    grep "$1" /tmp/results.txt | tr -s ' ' | awk -F' ' '{print $NF}'
}

run () {
    circuit=$1
    issigma=$2

    name=$(basename "${circuit}" | cut -d'.' -f1)
    # Skip some circuits
    if [[ $name =~ ^f ]]; then
        return
    fi
    symlen=$(echo "${name}" | cut -d'_' -f4)
    if [[ $issigma == y ]]; then
        flags="--sigma --symlen ${symlen} --npowers 8"
    fi

    mode=$(basename "${circuit}" | cut -d'.' -f2)
    if [[ $mode != dsl && ! $mode =~ ^o(1|2|3) ]]; then
        return
    fi
    # if [[ ! $mode =~ ^c2(a|v)$ && $mode != dsl && ! $mode =~ ^o(1|2|3)$ ]]; then
    #     mode=""
    # fi
    $prog obf get-kappa --scheme LIN --verbose $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lin1=$(get "κ = ")
        lin_enc=$(get "* # encodings")
    else
       lin1="[overflow]"
    fi
    $prog obf get-kappa --scheme LIN --verbose --smart $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lin2=$(get "κ = ")
    else
       lin2="[overflow]"
    fi
    $prog obf get-kappa --scheme LZ --verbose $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lz1=$(get "κ = ")
        lz_enc=$(get "* # encodings")
    else
        lz1="[overflow]"
    fi
    $prog obf get-kappa --scheme LZ --verbose --smart $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        lz2=$(get "κ = ")
    else
        lz2="[overflow]"
    fi
    $prog obf get-kappa --scheme MIFE --verbose $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        mife1=$(get "κ = ")
        mife_enc=$(get "* # encodings")
    else
        mife1="[overflow]"
    fi
    $prog obf get-kappa --scheme MIFE --verbose --smart $flags "$circuit" &>/tmp/results.txt
    if [ $? -eq 0 ]; then
        mife2=$(get "κ = ")
    else
        mife2="[overflow]"
    fi
    ninputs=$(get "ninputs")
    nconsts=$(get "nconsts")
    noutputs=$(get "noutputs")
    ngates=$(get "* ngates")
    nmuls=$(get " *nmuls")
    depth=$(get "* depth")
    degree=$(get "* degree")
    echo "$name, $mode, $ninputs, $nconsts, $noutputs, $ngates, $nmuls, $depth, $degree,    " \
         "$lin_enc, $lz_enc, $mife_enc,    " \
         "$lin1 | $lin2, $lz1 | $lz2, $mife1 | $mife2"
    rm -f "$circuit.obf"
}

echo "name, mode, nins, nkey, nouts, ngates, nmuls, depth, degree, nencodings, lin.nenc, lz.nenc, mife.nenc, lin.κ, lz.κ, mife.κ"
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
