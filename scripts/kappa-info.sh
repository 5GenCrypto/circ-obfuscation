#!/usr/bin/env bash

set -e

if [ x$1 == x ]; then
    echo "error: missing filename"
    echo "usage: $0 <filename>"
    exit 1
fi

pretty () {
    if [ ${#1} -gt 6 ]; then
        echo $(printf %.1e $1)
    else
        echo $(printf "%'.f" $1)
    fi
}

count=0

while read input; do
    line=$(echo $input | tr -d ' ')
    name=$(echo $line | cut -d',' -f1)
    if [ x"$name" == xname ]; then
        continue
    fi
    if [ x"$(echo $name | cut -d'_' -f1)" == x"ggm" ]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo $line | cut -d',' -f2)
    if [ x"$mode" != xdsl ]; then
        continue
    fi
    lin=$(echo $line | cut -d',' -f10)
    lz=$(echo $line | cut -d',' -f11)
    lin1=$(echo $lin | cut -d'|' -f1)
    lin2=$(echo $lin | cut -d'|' -f2)
    lz1=$(echo $lz | cut -d'|' -f1)
    lz2=$(echo $lz | cut -d'|' -f2)
    if [ x"$lin1" == x"[overflow]" ]; then
        lin=$lin1
    else
        lin1=$(pretty $lin1)
        lin2=$(pretty $lin2)
        lin="$lin1 ($lin2)"
    fi
    if [ x"$lz1" == x"[overflow]" ]; then
        lz=$lz1
    else
        lz1=$(pretty $lz1)
        lz2=$(pretty $lz2)
        lz="$lz1 ($lz2)"
    fi
    if [ $(expr $count % 2) == 1 ]; then
        echo "\rowcol \texttt{$name} $add && $lin && $lz \\\\"
    else
        echo "\texttt{$name} $add && $lin && $lz \\\\"
    fi
    count=$(expr $count + 1)
done < $1
