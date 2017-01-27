#!/usr/bin/env bash

set -e

if [ x"$1" == x"-m" ]; then
    usemodes=1
else
    usemodes=0
fi

while read input; do
    line=$(echo $input | tr -d ' ')
    name=$(echo $line | cut -d',' -f1)
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [ x"$name" == xname ]; then
        continue
    fi
    mode=$(echo $line | cut -d',' -f2)
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
    if [ $usemodes -eq 1 ]; then
        echo "\texttt{$name} && $mode && $ninputs && $nconsts && $nouts && $size && $nmuls && $depth && $degree \\\\"
    else
        echo "\texttt{$name} && $ninputs && $nconsts && $nouts && $size && $nmuls && $depth && $degree \\\\"
    fi
done
