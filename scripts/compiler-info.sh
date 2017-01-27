#!/usr/bin/env bash

set -e

usage () {
    echo "usage: compiler-info.sh"
    exit $1
}

pretty () {
    if [ $1 == "[overflow]" ]; then
        echo $1
    elif [ ${#1} -gt 6 ]; then
        echo $(printf %.1e $1)
    else
        echo $(printf "%'.f" $1)
    fi
}

while read input; do
    line=$(echo $input | tr -d ' ')
    name=$(echo $line | cut -d',' -f1)
    if [ x"$name" == xname ]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo $line | cut -d',' -f2)
    if [ x$mode == xc2a ]; then
        if [ x$modes != xc2ac2cdsl ]; then
            modes=
        fi
        results="\texttt{$name} "
    fi
    modes="$modes$mode"
    size=$(pretty $(echo $line | cut -d',' -f6))
    nmuls=$(pretty $(echo $line | cut -d',' -f7))
    deg=$(pretty $(echo $line | cut -d',' -f9))
    kappa=$(pretty $(echo $line | cut -d',' -f11 | cut -d'|' -f2))
    results="$results && $size & $nmuls & $deg & $kappa"
    if [ x$modes == xc2ac2cdsl ]; then
        results="$results \\\\"
        echo $results
        results=
        modes=
    fi
done < $1
