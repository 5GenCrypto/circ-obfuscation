#!/usr/bin/env bash

set -e

fname=${1:-kappas.csv}

count=0
curname=

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    if [[ ! $name =~ ^aes1r ]]; then
        continue
    fi
    if [[ $name =~ ^aes1r_(4|16|64) ]]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode == "" ]]; then
        continue
    fi
    circ=
    if [[ $name != "$curname" ]]; then
        if [[ $curname != "" ]]; then
            echo "\midrule"
        fi
        curname=$name
    fi
    size=$(echo "$line" | cut -d',' -f6)
    nmuls=$(echo "$line" | cut -d',' -f7)
    kappa=$(echo "$line" | cut -d',' -f11 | cut -d'|' -f2)
    case $mode in
        c2a )
            mode="\CTA" ;;
        c2v )
            mode="\CTV" ;;
        dsl )
            mode="\DSL" ;;
        opt-1 )
            mode="\DSL\ -O1" ;;
        opt-2 )
            mode="\DSL\ -O2" ;;
    esac
    if [[ $count != 0 && $((count % 2)) -eq 0 ]]; then
        echo "\rowcolor{white}"
    fi
    count=$((count + 1))
    if [[ $count != 0 && $((count % 5)) -eq 0 ]]; then
        echo "\multirow{-5}{*}{\texttt{$name}}"
    fi
    echo "$circ && $mode && \num{$size} & \num{$nmuls} & \num{$kappa} \tabularnewline"
done < "$fname"
