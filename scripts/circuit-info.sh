#!/usr/bin/env bash
#
# Prints circuit info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

if [ x"$1" = x"" ]; then
    fname='kappas.csv'
else
    fname=$1
fi

count=0

while read input; do
    line=$(echo $input | tr -d ' ')
    name=$(echo $line | cut -d',' -f1)
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [ x"$name" == xname ]; then
        continue
    fi
    mode=$(echo $line | cut -d',' -f2)
    if [ x"$mode" != xdsl ]; then
        continue
    fi
    ninputs=$(echo $line | cut -d',' -f3)
    nconsts=$(echo $line | cut -d',' -f4)
    nconsts=$(printf "%'.f" $nconsts)
    nouts=$(echo $line | cut -d',' -f5)
    size=$(echo $line | cut -d',' -f6)
    size=$(printf "%'.f" $size)
    nmuls=$(echo $line | cut -d',' -f7)
    nmuls=$(printf "%'.f" $nmuls)
    depth=$(echo $line | cut -d',' -f8)
    degree=$(echo $line | cut -d',' -f9)
    if [ ${#degree} -gt 6 ]; then
        degree=$(printf %.2e $degree)
    else
        degree=$(printf "%'.f" $degree)
    fi
    if [ $(expr $count % 2) -eq 1 ]; then
        echo -n "\\rowcol "
    fi
    echo "\texttt{$name} && $ninputs && $nconsts && $nouts && $size && $nmuls && $depth && $degree \\\\"
    count=$(expr $count + 1)
done < $fname
