#!/usr/bin/env bash
#
# Prints circuit info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

if [[ $1 == "" ]]; then
    fname="kappas.csv"
else
    fname="$1"
fi

count=0
curname=
curmode=

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [[ $name == name || $name =~ ^b0\\_(3|5|6|7)$ ]]; then
        continue
    fi
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode != dsl && $mode != opt ]]; then
        continue
    fi
    if [[ $curname == "" ]]; then
        curname=$name
        curmode=$mode
    fi
    if [[ $name != "$curname" && $result != "" ]]; then
        row=
        if (( $((count % 2)) == 1 )); then
            row="\\rowcol"
        fi
        circ="\texttt{$curname}"
        if [[ $curmode == opt ]]; then
            circ="$circ\$^*\$"
        fi
        echo "$row $circ && $result"
        curname=$name
        count=$((count + 1))
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
row=
if (( $((count % 2)) == 1 )); then
    row="\\rowcol"
fi
circ="\texttt{$curname}"
if [[ $curmode == opt ]]; then
    circ="$circ\$^*\$"
fi
echo "$row $circ && $result"
