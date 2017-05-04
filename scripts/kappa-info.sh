#!/usr/bin/env bash
#
# Prints kappa info for the DSL compiler in latex for a CSV file containing info
# produced by get-kappas.sh
#

set -e

fname=${1:-kappas.csv}

pretty () {
    if [ ${#1} -gt 6 ]; then
        printf %.1e "$1"
    else
        printf "%'.f" "$1"
    fi
}

kappas () {
    if [[ $1 == "[overflow]" ]]; then
        echo "$1"
    elif [[ $1 == "_" ]]; then
        echo "+"
    else
        a="\num{$1}"
        b="\num{$2}"
        if [[ $a == "$b" ]]; then
            echo "$a"
        else
            echo "$a ($b)"
        fi
    fi
}

count=0

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    if [[ $name == name || $name =~ ^f || $name =~ ^ggm_sigma || $name =~ ^aes1r_(3|5|6|7) || $name =~ ^prg ]]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode != o1 ]]; then
        continue
    fi
    lin=$(echo "$line" | cut -d',' -f10)
    lz=$(echo "$line" | cut -d',' -f11)
    lin1=$(echo "$lin" | cut -d'|' -f1)
    lin2=$(echo "$lin" | cut -d'|' -f2)
    lz1=$(echo "$lz" | cut -d'|' -f1)
    lz2=$(echo "$lz" | cut -d'|' -f2)
    lin=$(kappas "$lin1" "$lin2")
    lz=$(kappas "$lz1" "$lz2")
    if [[ "$((count % 2))" == 1 ]]; then
        echo "\rowcol \texttt{$name} && $lin && $lz \\\\"
    else
        echo "\texttt{$name} && $lin && $lz \\\\"
    fi
    count=$((count + 1))
done < "$fname"
