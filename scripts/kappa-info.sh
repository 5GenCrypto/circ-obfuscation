#!/usr/bin/env bash
#
# Prints kappa info for the DSL compiler in latex for a CSV file containing info
# produced by get-kappas.sh
#

set -e

if [ x"$1" == x ]; then
    fname="kappas.csv"
else
    fname="$1"
fi

pretty () {
    if [ ${#1} -gt 6 ]; then
        printf %.1e "$1"
    else
        printf "%'.f" "$1"
    fi
}

count=0

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    if [ x"$name" == xname ]; then
        continue
    fi
    if [ x"$(echo $name | cut -d'_' -f1)" == x"ggm" ]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo "$line" | cut -d',' -f2)
    if [ x"$mode" != xdsl ]; then
        continue
    fi
    lin=$(echo "$line" | cut -d',' -f10)
    lz=$(echo "$line" | cut -d',' -f11)
    lin1=$(echo "$lin" | cut -d'|' -f1)
    lin2=$(echo "$lin" | cut -d'|' -f2)
    lz1=$(echo "$lz" | cut -d'|' -f1)
    lz2=$(echo "$lz" | cut -d'|' -f2)
    if [ x"$lin1" == x"[overflow]" ]; then
        lin=$lin1
    else
        lin1=$(pretty "$lin1")
        lin2=$(pretty "$lin2")
        if [ "$lin1" == "$lin2" ]; then
            lin="$lin1"
        else
            lin="$lin1 ($lin2)"
        fi
    fi
    if [ x"$lz1" == x"[overflow]" ]; then
        lz=$lz1
    else
        lz1=$(pretty "$lz1")
        lz2=$(pretty "$lz2")
        if [ "$lz1" == "$lz2" ]; then
            lz="$lz1"
        else
            lz="$lz1 ($lz2)"
        fi
    fi
    if [ "$((count % 2))" == 1 ]; then
        echo "\rowcol \texttt{$name} && $lin && $lz \\\\"
    else
        echo "\texttt{$name} && $lin && $lz \\\\"
    fi
    count=$((count + 1))
done < "$fname"
