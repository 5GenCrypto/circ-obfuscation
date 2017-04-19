#!/usr/bin/env bash
#
# Prints GGM circuit info in latex for a CSV file containing info produced by
# get-kappas.sh
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

scheme () {
    if [[ $1 == "[overflow]" ]]; then
        out=$1
    else
        a=$(pretty "$1")
        b=$(pretty "$2")
        if [[ $a == "$b" ]]; then
            out=$a
        else
            out="$a ($b)"
        fi
    fi
    echo "$out"
}

count=0
total=0
declare -A row
while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    # skip column names row
    if [[ $name == name ]]; then
        continue
    fi
    # skip all non-ggm circuits
    if [[ ! $name =~ ^ggm ]]; then
        continue
    fi
    if [[ $name =~ ^ggm_sigma ]]; then
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
    mode=$(echo "$line" | cut -d',' -f2)
    # skip non-dsl compiled circuits
    if [[ $mode != dsl ]]; then
        continue
    fi
    lin=$(echo "$line" | cut -d',' -f10 | cut -d'|' -f2)
    lz=$(echo "$line" | cut -d',' -f11 | cut -d'|' -f2)
    lin=$(scheme "$lin" "$lin")
    lz=$(scheme "$lz" "$lz")
    if [ $sigma -eq 1 ]; then
        index=$((count - total))
        row[$index]="${row[$index]} && $lin && $lz \\\\"
        if [ $((count % 2)) == 1 ]; then
            echo "\rowcol ${row[$index]}"
        else
            echo "${row[$index]}"
        fi
    else
        row[$count]="$ninputs & $noutputs && $lin && $lz"
    fi
    count=$((count + 1))
done < "$fname"
