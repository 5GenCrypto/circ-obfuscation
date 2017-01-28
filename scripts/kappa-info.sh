#!/usr/bin/env bash

set -e

usage () {
    echo "usage: kappa-info.sh [ggm]"
    exit $1
}

pretty () {
    if [ ${#1} -gt 6 ]; then
        echo $(printf %.1e $1)
    else
        echo $(printf "%'.f" $1)
    fi
}

if [ x"$1" == xggm ]; then
    while read input; do
        line=$(echo $input | tr -d ' ')
        name=$(echo $line | cut -d',' -f1)
        if [ x"$name" == xname ]; then
            continue
        fi
        if [ x"$(echo $name | cut -d'_' -f1)" != x"ggm" ]; then
            continue
        fi
        if [ x"$(echo $name | cut -d'_' -f2)" == x"sigma" ]; then
            sigma=1
        else
            sigma=0
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
            if [ x$lz1 == x$lz2 ]; then
                lz=$lz1
            else
                lz="$lz1 ($lz2)"
            fi
        fi
        if [ $sigma -eq 1 ]; then
            add="&&&&"
        else
            add=""
        fi
        echo "\texttt{$name} $add && $lin && $lz \\\\"
    done
elif [ x"$1" == x ]; then
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
        echo "\texttt{$name} $add && $lin && $lz \\\\"
    done
else
    echo "error: unknown argument '$1'"
    usage 1
fi
