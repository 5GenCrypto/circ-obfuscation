#!/usr/bin/env bash
#
# Test all circuits
#

scriptdir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
prog=$(readlink -f "$scriptdir/../mio.sh")
circuits=$(readlink -f "$scriptdir/../circuits")

run () {
    $prog --mmap "$2" --scheme LZ "$1" --smart $3
}

obf_test () {
    echo ""
    echo "***"
    echo "***"
    echo "*** OBF $1 $2"
    echo "***"
    echo "***"
    echo ""
    run "$1" "$2" "--smart --obf-test"
}

obf_test_sigma () {
    echo ""
    echo "***"
    echo "***"
    echo "*** OBF SIGMA $1 $2"
    echo "***"
    echo "***"
    echo ""
    run "$1" "$2" "--smart --obf-test --sigma --symlen 16"
}

mife_test () {
    echo ""
    echo "***"
    echo "***"
    echo "*** MIFE $1 $2"
    echo "***"
    echo "***"
    echo ""
    run "$1" "$2" "--smart --mife-test"
}

obf_test_sigma () {
    echo ""
    echo "***"
    echo "***"
    echo "*** MIFE SIGMA $1 $2"
    echo "***"
    echo "***"
    echo ""
    run "$1" "$2" "--smart --mife-test --sigma --symlen 16"
}

for circuit in $circuits/*.acirc; do
    obf_test "$circuit" DUMMY
done

for circuit in $circuits/sigma/*.acirc; do
    obf_test_sigma "$circuit" DUMMY
done

for circuit in $circuits/*.acirc; do
    mife_test "$circuit" DUMMY
done

for circuit in $circuits/sigma/*.acirc; do
    mife_test_sigma "$circuit" DUMMY
done
