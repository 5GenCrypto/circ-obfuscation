#!/usr/bin/env bash
#
# Test all circuits
#

scriptdir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
prog=$(readlink -f "$scriptdir/../mio.sh")
circuits=$(readlink -f "$scriptdir/../circuits")

set -e

obf_test () {
    echo ""
    echo "***"
    echo "***"
    echo "*** OBF $1 $2 $3"
    echo "***"
    echo "***"
    echo ""
    $prog obf test --smart --mmap $2 --scheme $3 $1
}

obf_test_sigma () {
    echo ""
    echo "***"
    echo "***"
    echo "*** OBF SIGMA $1 $2 $3"
    echo "***"
    echo "***"
    echo ""
    $prog obf test --smart --sigma --symlen 16 --mmap $2 --scheme $3 $1
}

mife_test () {
    echo ""
    echo "***"
    echo "***"
    echo "*** MIFE $1 $2"
    echo "***"
    echo "***"
    echo ""
    $prog mife test --smart --mmap $2 $1
}

mife_test_sigma () {
    echo ""
    echo "***"
    echo "***"
    echo "*** MIFE SIGMA $1 $2"
    echo "***"
    echo "***"
    echo ""
    $prog mife test --smart --sigma --symlen 16 -mmap $2 $1
}

for circuit in $circuits/*.acirc; do
    mife_test "$circuit" DUMMY
done

for circuit in $circuits/sigma/*.acirc; do
    mife_test_sigma "$circuit" DUMMY
done

for circuit in $circuits/*.acirc; do
    obf_test "$circuit" DUMMY LZ
    obf_test "$circuit" DUMMY MIFE
done

for circuit in $circuits/sigma/*.acirc; do
    obf_test_sigma "$circuit" DUMMY LZ
    obf_test_sigma "$circuit" DUMMY MIFE
done
