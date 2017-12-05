#!/usr/bin/env bash
#
# Prints circuit info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

fname=${1:-kappas.csv}
source "$(dirname "$0")/utils.sh"

count=0
curname=
curmode=

printline () {
    row=
    if (( $((count % 2)) == 1 )); then
        row="\\rowcol"
    fi
    circ="\texttt{$curname}"
    if [[ $curmode == o2 ]]; then
        circ="$circ\$^{*}\$"
    elif [[ $curmode == o3 ]]; then
        circ="$circ\$^{**}\$"
    fi
    echo "$row $circ && $result"
    curname=$name
    count=$((count + 1))
    
}

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(get_name "$line")
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [[ $name == name || $name =~ ^aes1r\\_(3|5|6|7)$ || $name =~ ^f ]]; then
        continue
    fi
    if [[ $name =~ ^prg  || $name == "sbox" || $name =~ ^gf || $name == linearParts ]]; then
        continue
    fi
    mode=$(get_mode "$line")
    if [[ $mode != o1 && $mode != o2 && $mode != o3  &&  ! $name =~ ^ggm\\_sigma ]]; then
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
    ninputs=$(get_ninputs "$line")
    nconsts=$(get_nconsts "$line")
    nouts=$(get_nouts "$line")
    size=$(get_size "$line")
    nmuls=$(get_nmuls "$line")
    depth=$(get_depth "$line")
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
    result="\num{$ninputs} && \num{$nconsts} && \num{$nouts} && \num{$size} && \num{$nmuls} && \num{$depth} && $degree && $kappa \\\\"
done < "$fname"
printline
