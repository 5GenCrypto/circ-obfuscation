#!/usr/bin/env bash
#
# Prints compiler info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

if [ x"$1" = x"" ]; then
    fname='kappas.csv'
else
    fname=$1
fi

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
minnmuls=$minsize
minkappa=$minsize
count=0

while read input; do
    line=$(echo $input | tr -d ' ')
    name=$(echo $line | cut -d',' -f1)
    if [ x$name == xname ] || [ x$name == xf3_4 ] || [ x$name == xmapper_8 ]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo $line | cut -d',' -f2)
    if [ x$mode = xc2a ]; then
        if [ x$modes != xc2ac2vdsl ]; then
            modes=
            minsize=10000000000
            mindeg=$minsize
            minnmuls=$minsize
            minkappa=$minsize
        fi
        results=""
    fi
    modes=$modes$mode
    if [ x$modes = xdsl ]; then
        modes=""
        continue
    fi
    size=$(echo $line | cut -d',' -f6)
    if [ $size -lt $minsize ]; then
        minsize=$size
    fi
    size=$(pretty $size)
    nmuls=$(echo $line | cut -d',' -f7)
    if [ $nmuls -lt $minnmuls ]; then
        minnmuls=$nmuls
    fi
    nmuls=$(pretty $nmuls)
    deg=$(echo $line | cut -d',' -f9)
    if [ $deg -lt $mindeg ]; then
        mindeg=$deg
    fi
    deg=$(pretty $deg)
    kappa=$(echo $line | cut -d',' -f11)
    if [ $kappa -lt $minkappa ]; then
        minkappa=$kappa
    fi
    kappa=$(pretty $kappa)
    if [ x$mode == xc2a ]; then
        results="texttt{$name} && $size & $nmuls & $deg & $kappa "
    elif [ x$mode == xc2v ]; then
        results="$results && $size & $nmuls & $deg & $kappa"
    else
        results="$results && $size & $nmuls & $deg & $kappa "
    fi
    if [ x$modes == xc2ac2vdsl ]; then
        minsize=$(pretty $minsize)
        minnmuls=$(pretty $minnmuls)
        mindeg=$(pretty $mindeg)
        minkappa=$(pretty $minkappa)
        results="$results"
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/ && $minsize & / && textbf{$minsize} & /g; print \$r")
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $mindeg / textbf{$mindeg} /g; print \$r")
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minnmuls / textbf{$minnmuls} /g; print \$r")
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minkappa / textbf{$minkappa} /g; print \$r")
        results=$(perl -e "\$r = \"$results\"; \$r =~ s/_/\\\_/g; \$r =~ s/text/\\\text/g; print \$r")
        if [ $(expr $count % 2) -eq 1 ]; then
            results="\\rowcol $results"
        fi
        echo "$results \\\\"
        results=
        modes=
        minsize=10000000000
        mindeg=$minsize
        minkappa=$minsize
        count=$(expr $count + 1)
    fi
done < $fname
