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

minsize=1000000000
mindeg=$minsize
minkappa=$minsize

while read input; do
    line=$(echo $input | tr -d ' ')
    name=$(echo $line | cut -d',' -f1)
    if [ x"$name" == xname ]; then
        continue
    elif [ x"$name" == xf3_4 ]; then
        continue
    elif [ x"$name" == xmapper_8 ]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo $line | cut -d',' -f2)
    if [ x$mode == xc2a ]; then
        if [ x$modes != xc2ac2cdsl ]; then
            modes=
            minsize=10000000000
            mindeg=$minsize
            minkappa=$minsize
        fi
        results="texttt{$name}"
    fi
    modes="$modes$mode"
    size=$(echo $line | cut -d',' -f6)
    if [ $size -lt $minsize ]; then
        minsize=$size
    fi
    size=$(pretty $size)
    nmuls=$(pretty $(echo $line | cut -d',' -f7))
    deg=$(echo $line | cut -d',' -f9)
    if [ $deg -lt $mindeg ]; then
        mindeg=$deg
    fi
    kappa=$(echo $line | cut -d',' -f11 | cut -d'|' -f2)
    if [ $kappa -lt $minkappa ]; then
        minkappa=$kappa
    fi
    results="$results && $size & $nmuls & $deg & $kappa "
    if [ x$modes == xc2ac2cdsl ]; then
        results="$results"
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/ && $minsize & / && textbf{$(pretty $minsize)} & /g; print \$r")
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $mindeg / textbf{$(pretty $mindeg)} /g; print \$r")
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minkappa / textbf{$(pretty $minkappa)} /g; print \$r")
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/_/\\\_/g; \$r =~ s/text/\\\text/g; print \$r")
        echo "$results \\\\"
        results=
        modes=
    fi
done < $1
