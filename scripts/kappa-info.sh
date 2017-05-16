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
    name=$(get_name "$line")
    if [[ $name == name || $name =~ ^f || $name =~ ^ggm_sigma || $name =~ ^aes1r_ || $name =~ ^prg ]]; then
        continue
    fi
    if [[ $name =~ 64 || $name == sbox ]]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(get_mode "$line")
    if [[ $mode != o1 ]]; then
        continue
    fi
    lin_enc="\num{$(get_nencodings_lin "$line")}"
    lz_enc="\num{$(get_nencodings_lz "$line")}"
    mife_enc="\num{$(get_nencodings_mife "$line")}"

    lin=$(get_kappa_lin "$line")
    lz=$(get_kappa_lz "$line")
    mife=$(get_kappa_mife "$line")
    lin=$(pretty "$(echo "$lin" | cut -d'|' -f2)")
    lz=$(pretty "$(echo "$lz" | cut -d'|' -f2)")
    mife=$(pretty "$(echo "$mife" | cut -d'|' -f2)")
    # lin=$(kappas "$lin2" "$lin2")
    # lz=$(kappas "$lz2" "$lz2")
    # mife=$(kappas "$mife2" "$mife2")
    result="\texttt{$name} && $lin_enc & $lin && $lz_enc & $lz && $mife_enc & $mife \\\\"
    if [[ "$((count % 2))" == 1 ]]; then
        echo "\rowcol$result"
    else
        echo "$result"
    fi
    count=$((count + 1))
done < "$fname"
