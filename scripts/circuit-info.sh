#!/usr/bin/env bash
#
# Prints circuit info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

if [ x"$1" = x ]; then
    fname="kappas.csv"
else
    fname="$1"
fi

count=0

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [ x"$name" == xname ]; then
        continue
    fi
    if [[ $name =~ ^b0\\_(3|5|6|7)$ ]]; then
        continue
    fi
    mode=$(echo "$line" | cut -d',' -f2)
    if [ x"$mode" != xdsl ]; then
        continue
    fi
    ninputs="\num{$(echo "$line" | cut -d',' -f3)}"
    nconsts="\num{$(echo "$line" | cut -d',' -f4)}"
    # nconsts=$(printf "%'.f" "$nconsts")
    nouts="\num{$(echo "$line" | cut -d',' -f5)}"
    size="\num{$(echo "$line" | cut -d',' -f6)}"
    # size=$(printf "%'.f" "$size")
    nmuls="\num{$(echo "$line" | cut -d',' -f7)}"
    # nmuls=$(printf "%'.f" "$nmuls")
    depth="\num{$(echo "$line" | cut -d',' -f8)}"
    degree=$(echo "$line" | cut -d',' -f9)
    if [ ${#degree} -gt 6 ]; then
        degree=$(printf %.2e "$degree")
    else
        degree="\num{$degree}"
    fi
    kappa=$(echo "$line" | cut -d',' -f11 | cut -d'|' -f2)
    if [[ $kappa != "[overflow]" ]]; then
        kappa="\num{$kappa}"
    fi
    if [ $((count % 2)) -eq 1 ]; then
        echo -n "\\rowcol "
    fi
    echo "\texttt{$name} && $ninputs && $nconsts && $nouts && $size && $nmuls && $depth && $degree && $kappa \\\\"
    count=$((count + 1))
done < "$fname"
