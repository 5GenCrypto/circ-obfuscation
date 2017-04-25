#!/usr/bin/env bash
#
# Prints circuit info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

fname=${1:-kappas.csv}

count=0
curname=
curmode=

printline () {
    row=
    if (( $((count % 2)) == 1 )); then
        row="\\rowcol"
    fi
    circ="\texttt{$curname}"
    if [[ $curmode == opt-2 ]]; then
        circ="$circ\$^2\$"
    elif [[ $curmode == opt-3 ]]; then
        circ="$circ\$^3\$"
    fi
    echo "$row $circ && $result"
    curname=$name
    count=$((count + 1))
    
}

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [[ $name == name || $name =~ ^aes1r\\_(3|5|6|7)$ || $name =~ ^f ]]; then
        continue
    fi
    if [[ $name =~ ^prg  || $name == "sbox" ]]; then
        continue
    fi
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode != opt-1 && $mode != opt-2 && $mode != opt-3 ]]; then
        continue
    fi
    if [[ $curname == "" ]]; then
        curname=$name
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
printline
