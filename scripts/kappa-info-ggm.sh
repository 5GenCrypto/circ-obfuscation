#!/usr/bin/env bash
#
# Prints GGM circuit info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

if [ x"$1" = x"" ]; then
    fname='kappas.csv'
else
    fname=$1
fi

pretty () {
    if [ ${#1} -gt 6 ]; then
        echo $(printf %.1e $1)
    else
        echo $(printf "%'.f" $1)
    fi
}

scheme () {
    if [ x$1 == x"[overflow]" ]; then
        out=$1
    else
        a=$(pretty $1)
        b=$(pretty $2)
        if [ x$a == x$b ]; then
            out=$a
        else
            out="$a ($b)"
        fi
    fi
    echo $out
}

count=0
total=0
declare -A row
while read input; do
    line=$(echo $input | tr -d ' ')
    name=$(echo $line | cut -d',' -f1)
    # skip column names row
    if [ x$name == xname ]; then
        continue
    fi
    # skip all non-ggm circuits
    if [ x$(echo $name | cut -d'_' -f1) != xggm ]; then
        continue
    fi
    if [ x$(echo $name | cut -d'_' -f2) == xsigma ]; then
        sigma=1
        if [ $total == 0 ]; then
            total=$count
        fi
    else
        sigma=0
        ninputs=$(echo $line | cut -d',' -f3)
        noutputs=$(echo $line | cut -d',' -f5)
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo $line | cut -d',' -f2)
    # skip non-dsl compiled circuits
    if [ x"$mode" != xdsl ]; then
        continue
    fi
    lin=$(echo $line | cut -d',' -f10)
    lz=$(echo $line | cut -d',' -f11)
    lin1=$(echo $lin | cut -d'|' -f1)
    lin2=$(echo $lin | cut -d'|' -f2)
    lz1=$(echo $lz | cut -d'|' -f1)
    lz2=$(echo $lz | cut -d'|' -f2)
    lin=$(scheme $lin2 $lin2)
    lz=$(scheme $lz2 $lz2)
    if [ $sigma -eq 1 ]; then
        index=$(($count - $total))
        row[$index]="${row[$index]} && $lin && $lz \\\\"
        if [ $(($count % 2)) == 1 ]; then
            echo "\rowcol ${row[$index]}"
        else
            echo "${row[$index]}"
        fi
    else
        row[$count]="$ninputs & $noutputs && $lin && $lz"
    fi
    count=$(($count + 1))
done < $fname
