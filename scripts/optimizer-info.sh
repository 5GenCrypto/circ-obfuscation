#!/usr/bin/env bash
#
# Prints optimizer info in latex from a CSV file containing info produced by
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
    row=$( ((count % 2 == 1)) && echo "\\rowcol" || echo "" )
    circ="\texttt{$curname}"
    minsize=$(min "$dslsize" "$optsize")
    minmuls=$(min "$dslmuls" "$optmuls")
    minkappa=$(min "$dslkappa" "$optkappa")
    minsize=$(pretty "$minsize")
    minmuls=$(pretty "$minmuls")
    minkappa=$(pretty "$minkappa")
    results="${circuits[dsl]} && ${circuits[opt]}"
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/ && $minsize & / && textbf{$minsize} & /g; print \$r")
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minmuls / textbf{$minmuls} /g; print \$r")
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minkappa / textbf{$minkappa} /g; print \$r")
    results=$(perl -e "\$r = \"$results\"; \$r =~ s/_/\\\_/g; \$r =~ s/text/\\\text/g; print \$r")
    echo "$row $circ && $results \\\\"
    count=$((count + 1))
}

count=0
curname=
declare -A circuits

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    if [[ $name == name ]]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode != dsl && $mode != opt ]]; then
        continue
    fi
    if [[ $name != "$curname" ]]; then
        if (( ${#circuits[@]} == 2)); then
            printline
        fi
        curname=$name
        unset -v dslsize dslmuls dslkappa
        unset -v optsize optmuls optkappa
        unset circuits
        declare -A circuits
    fi
    size=$(echo "$line" | cut -d',' -f6)
    muls=$(echo "$line" | cut -d',' -f7)
    kappa=$(echo "$line" | cut -d',' -f11 | cut -d'|' -f2)
    case $mode in
        dsl )
            dslsize=$size
            dslmuls=$muls
            dslkappa=$kappa
            ;;
        opt )
            optsize=$size
            optmuls=$muls
            optkappa=$kappa
            ;;
    esac
    size=$(pretty "$size")
    nmuls=$(pretty "$nmuls")
    kappa=$(pretty "$kappa")
    circuits[$mode]="$size & $muls & $kappa "
done < "$fname"

