#!/usr/bin/env bash

set -e

fname=${1:-kappas.csv}
source "$(dirname "$0")/utils.sh"

count=0
curname=
curmode=

getname () {
    if [[ $1 =~ linear ]]; then
        echo "linear"
    elif [[ $1 =~ xor-and ]]; then
        echo "xor-and"
    elif [[ $1 =~ xor-maj ]]; then
        echo "xor-maj"
    fi
}

printline () {
    old=$curname
    new=$(getname "$name")
    row=
    if (( $((count % 2)) == 1 )); then
        row="\\rowcol "
    fi
    circ="\texttt{$curname}"
    if [[ $curmode =~ ^o ]]; then
        circ="$circ\$^*\$"
    fi
    echo "$row$circ && $result"
    if [[ $old != "$new" && $1 == "" ]]; then
        echo "\midrule"
    fi
    curname=$new
    count=$((count + 1))
}

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(get_name "$line")
    if [[ ! $name =~ ^prg ]]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(get_mode "$line")
    if [[ $mode != dsl && ! $mode =~ ^o ]]; then
        continue
    fi
    if [[ $curname == "" ]]; then
        curname=$(getname "$name")
        curmode=$mode
    fi
    if [[ $name != "$curname" && $result != "" ]]; then
        printline
    fi
    curmode=$mode
    ninputs="\num{$(get_ninputs "$line")}"
    nconsts="\num{$(get_nconsts "$line")}"
    nouts="\num{$(get_nouts "$line")}"
    size="\num{$(get_size "$line")}"
    nmuls="\num{$(get_nmuls "$line")}"
    depth="\num{$(get_depth "$line")}"
    degree=$(get_degree "$line")
    if (( ${#degree} > 6 )); then
        degree=$(printf %.2e "$degree")
    else
        degree="\num{$degree}"
    fi
    kappa=$(get_kappa_mife "$line" | cut -d'|' -f2)
    if [[ $kappa != "[overflow]" ]]; then
        kappa="\num{$kappa}"
    fi
    result="$ninputs && $nconsts && $nouts && $size && $nmuls && $depth && $degree && $kappa \\\\"
done < "$fname"
printline 1
