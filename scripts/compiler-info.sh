#!/usr/bin/env bash
#
# Prints compiler info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

fname=${1:-kappas.csv}

pretty () {
    if [ x"$1" == x"[overflow]" ]; then
        echo "$1"
    elif [ ${#1} -gt 6 ]; then
        printf %.1e "$1"
    else
        printf "%'.f" "$1"
    fi
}

min () {
    echo "$@" | tr ' ' '\n' | sort -n | head -1
}

printline () {
    minsize=$(min $c2asize $c2vsize $dslsize)
    minnmuls=$(min $c2anmuls $c2vnmuls $dslnmuls)
    minkappa=$(min $c2akappa $c2vkappa $dslkappa)
    minsize=$(pretty "$minsize")
    minnmuls=$(pretty "$minnmuls")
    minkappa=$(pretty "$minkappa")
    row=
    if [ $((count % 2)) -eq 1 ]; then
        row="\\rowcol"
    fi
    circ="\texttt{$curname}"
    if [[ ${circuits[opt]} -eq 1 ]]; then
        circ="$circ\$^*\$"
    fi
    if [[ ${circuits[c2a]} == 1 ]]; then
        results="$results && $c2a"
    else
        results="$results && & &"
    fi
    if [[ ${circuits[c2v]} == 1 ]]; then
        results="$results && $c2v"
    else
        results="$results && & &"
    fi
    if [[ ${circuits[dsl]} == 1 ]]; then
        results="$results && $dsl "
    else
        results="$results && & &"
    fi
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/ && $minsize & / && B {$minsize} & /g; print \$r")
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minnmuls / B {$minnmuls} /g; print \$r")
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minkappa / B {$minkappa} /g; print \$r")
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/_/\\\_/g; \$r =~ s/text/\\\text/g; \$r =~ s/B/\\\B/g; print \$r")
    echo "$row $circ $results \\\\"
    count=$((count + 1))
}

count=0
curname=
declare -A circuits

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    if [[ $name == name || $name == f3_4 || $name =~ ^mapper.* || $name =~ ^linearParts$ ]]; then
        continue
    fi
    if [[ $name =~ ^b0_(3|5|6|7)$ ]]; then
        continue
    fi
    # Special case for inconsistent sbox naming
    if [[ $name == "sbox_" ]]; then
        name="sbox"
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [[ $curname == "" ]]; then
        curname=$name
    fi
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode == "" ]]; then
        continue
    fi
    if [[ $name != "$curname" ]]; then
        if [[ ${#circuits[@]} -gt 0 ]]; then
            printline
        fi
        curname=$name
        unset -v results modes
        unset -v c2asize c2anmuls c2akappa
        unset -v c2vsize c2vnmuls c2vkappa
        unset -v dslsize dslnmuls dslkappa
        unset circuits
        declare -A circuits
    fi
    circuits[$mode]=1
    size=$(echo "$line" | cut -d',' -f6)
    nmuls=$(echo "$line" | cut -d',' -f7)
    kappa=$(echo "$line" | cut -d',' -f11 | cut -d'|' -f2)
    case $mode in
        c2a )
            c2asize=$size
            c2anmuls=$nmuls
            c2akappa=$kappa
            ;;
        c2v )
            c2vsize=$size
            c2vnmuls=$nmuls
            c2vkappa=$kappa
            ;;
        dsl | opt )
            dslsize=$size
            dslnmuls=$nmuls
            dslkappa=$kappa
            ;;
    esac
    size=$(pretty "$size")
    nmuls=$(pretty "$nmuls")
    kappa=$(pretty "$kappa")
    if [[ $mode = c2a ]]; then
        c2a="$size & $nmuls & $kappa"
    elif [[ $mode = c2v ]]; then
        c2v="$size & $nmuls & $kappa "
    elif [[ $mode = dsl ]]; then
        dsl="$size & $nmuls & $kappa"
    elif [[ $mode = opt ]]; then
        dsl="$size & $nmuls & $kappa"
    fi
done < "$fname"
printline
