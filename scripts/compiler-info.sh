#!/usr/bin/env bash
#
# Prints compiler info in latex for a CSV file containing info produced by
# get-kappas.sh
#

set -e

if [ x"$1" = x ]; then
    fname="kappas.csv"
else
    fname="$1"
fi

pretty () {
    if [ x"$1" == x"[overflow]" ]; then
        echo "$1"
    elif [ ${#1} -gt 6 ]; then
        printf %.1e "$1"
    else
        printf "%'.f" "$1"
    fi
}

minsize=1000000000
minnmuls=$minsize
minkappa=$minsize
count=0
curname=

declare -A circuits=()

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    if [[ $name == name || $name == f3_4 || $name == mapper_8 ]]; then
        continue
    fi
    if [[ $name =~ ^b0_(3|5|6|7)$ ]]; then
        continue
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
            # Print existing results
            minsize=$(pretty "$minsize")
            minnmuls=$(pretty "$minnmuls")
            minkappa=$(pretty "$minkappa")
            results="$results"
            if [[ ${circuits[c2a]} == 1 ]]; then
                results="$results && $c2a"
            else
                results="$results && & & "
            fi
            if [[ ${circuits[c2v]} == 1 ]]; then
                results="$results && $c2v"
            else
                results="$results && & & "
            fi
            if [[ ${circuits[dsl]} == 1 ]]; then
                results="$results && $dsl "
            else
                results="$results && & & "
            fi
            results=$(perl -e "\$r = \"$results\"; \$r =~ s/ && $minsize & / && textbf{$minsize} & /g; print \$r")
            results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minnmuls / textbf{$minnmuls} /g; print \$r")
            results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minkappa / textbf{$minkappa} /g; print \$r")
            results=$(perl -e "\$r = \"$results\"; \$r =~ s/_/\\\_/g; \$r =~ s/text/\\\text/g; print \$r")
            if [ $((count % 2)) -eq 1 ]; then
                results="\\rowcol $results"
            fi
            echo "$results \\\\"
            count=$((count + 1))
        fi
        results=
        modes=
        minsize=10000000000
        minnmuls=$minsize
        minkappa=$minsize
        curname=$name
        unset circuits
        declare -A circuits
    fi
    circuits[$mode]=1
    size=$(echo "$line" | cut -d',' -f6)
    if [ "$size" -lt $minsize ]; then
        minsize=$size
    fi
    size=$(pretty "$size")
    nmuls=$(echo "$line" | cut -d',' -f7)
    if [ "$nmuls" -lt $minnmuls ]; then
        minnmuls=$nmuls
    fi
    nmuls=$(pretty "$nmuls")
    kappa=$(echo "$line" | cut -d',' -f11 | cut -d'|' -f2)
    if [ "$kappa" -lt $minkappa ]; then
        minkappa=$kappa
    fi
    kappa=$(pretty "$kappa")
    if [[ ${#circuits[@]} = 1 ]]; then
        results="texttt{$name}"
    fi
    if [[ $mode = c2a ]]; then
        c2a="$size & $nmuls & $kappa"
    elif [[ $mode = c2v ]]; then
        c2v="$size & $nmuls & $kappa "
    elif [[ $mode = dsl ]]; then
        dsl="$size & $nmuls & $kappa"
    elif [[ $mode = opt ]]; then
        dsl="$size & $nmuls & $kappa"
    fi
done < $fname
minsize=$(pretty "$minsize")
minnmuls=$(pretty "$minnmuls")
minkappa=$(pretty "$minkappa")
results="$results"
if [[ ${circuits[c2a]} == 1 ]]; then
    results="$results && $c2a"
else
    results="$results && & & "
fi
if [[ ${circuits[c2v]} == 1 ]]; then
    results="$results && $c2v"
else
    results="$results && & & "
fi
if [[ ${circuits[dsl]} == 1 ]]; then
    results="$results && $dsl "
else
    results="$results && & & "
fi
results=$(perl -e "\$r = \"$results\"; \$r =~ s/ && $minsize & / && textbf{$minsize} & /g; print \$r")
results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minnmuls / textbf{$minnmuls} /g; print \$r")
results=$(perl -e "\$r = \"$results\"; \$r =~ s/ $minkappa / textbf{$minkappa} /g; print \$r")
results=$(perl -e "\$r = \"$results\"; \$r =~ s/_/\\\_/g; \$r =~ s/text/\\\text/g; print \$r")
if [ $((count % 2)) -eq 1 ]; then
    results="\\rowcol $results"
fi
echo "$results \\\\"
