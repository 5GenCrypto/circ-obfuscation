#!/usr/bin/env bash

set -e

fname=${1:-kappas.csv}

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
    if [[ $curmode == opt ]]; then
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
    name=$(echo "$line" | cut -d',' -f1)
    if [[ ! $name =~ ^prg ]]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode != dsl && $mode != opt ]]; then
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
    ninputs="\num{$(echo "$line" | cut -d',' -f3)}"
    nconsts="\num{$(echo "$line" | cut -d',' -f4)}"
    nouts="\num{$(echo "$line" | cut -d',' -f5)}"
    size="\num{$(echo "$line" | cut -d',' -f6)}"
    nmuls="\num{$(echo "$line" | cut -d',' -f7)}"
    depth="\num{$(echo "$line" | cut -d',' -f8)}"
    degree=$(echo "$line" | cut -d',' -f9)
    if (( ${#degree} > 6 )); then
        degree=$(printf %.2e "$degree")
    else
        degree="\num{$degree}"
    fi
    kappa=$(echo "$line" | cut -d',' -f11 | cut -d'|' -f2)
    if [[ $kappa != "[overflow]" ]]; then
        kappa="\num{$kappa}"
    fi
    result="$ninputs && $nconsts && $nouts && $size && $nmuls && $depth && $degree && $kappa \\\\"
done < "$fname"
printline 1
