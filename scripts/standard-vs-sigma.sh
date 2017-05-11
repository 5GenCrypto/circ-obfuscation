#!/usr/bin/env bash

set -e

fname=${1:-kappas.csv}
source "$(dirname "$0")/utils.sh"

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
    elif [[ $1 == "_" ]]; then
        out=""
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
    name=$(get_name "$line")
    # skip column names row
    if [[ $name == name ]]; then
        continue
    fi
    # skip all non-ggm circuits
    if [[ ! $name =~ ^ggm ]]; then
        continue
    fi
    if [[ $name =~ ^ggm_sigma ]]; then
        if [[ ! $name =~ ^ggm_sigma_(1|2|3|4)_16_128 ]]; then
            continue
        fi
        sigma=1
        if [ $total == 0 ]; then
            total=$count
        fi
    else
        sigma=0
        ninputs=$(echo $line | cut -d',' -f3)
        noutputs=$(echo $line | cut -d',' -f5)
        if [[ $noutputs != 128 ]]; then
            continue
        fi
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(get_mode "$line")
    # skip non-dsl compiled circuits
    if [[ $mode != dsl ]]; then
        continue
    fi
    mife=$(get_kappa_mife "$line" | cut -d'|' -f2)
    if [ $sigma -eq 1 ]; then
        index=$((count - total))
        row[$index]="${row[$index]} && $mife \\\\"
        if [ $((count % 2)) == 1 ]; then
            echo "\rowcol ${row[$index]}"
        else
            echo "${row[$index]}"
        fi
    else
        row[$count]="$ninputs & $noutputs && $mife"
    fi
    count=$((count + 1))
done < "$fname"
