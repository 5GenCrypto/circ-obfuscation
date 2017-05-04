#!/usr/bin/env bash

set -e

fname=${1:-kappas.csv}

circcount=0
count=0
curname=

printline () {
    echo " && $mode && \num{$size} & \num{$nmuls} & \num{$kappa} \tabularnewline"
}

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(echo "$line" | cut -d',' -f1)
    if [[ ! $name =~ ^aes1r ]]; then
        continue
    fi
    # if [[ $name =~ ^aes1r_(4|16) ]]; then
    #     continue
    # fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [[ $curname != "" ]]; then
        if [[ $name != "$curname" ]]; then
            echo "\multirow{-$circcount}{*}{\texttt{$curname}}"
        fi
        printline
    fi
    mode=$(echo "$line" | cut -d',' -f2)
    if [[ $mode == "" ]]; then
        continue
    fi
    case $mode in
        c2a )
            mode="\CTA" ;;
        c2v )
            mode="\CTV" ;;
        dsl )
            mode="\DSL" ;;
        o1 )
            mode="\DSL\ -O1" ;;
        o2 )
            mode="\DSL\ -O2" ;;
        o3 )
            mode="\DSL\ -O3" ;;
    esac
    if [[ $name != "$curname" ]]; then
        if [[ $curname != "" ]]; then
            echo "\midrule"
        fi
        circcount=0
        curname=$name
    fi
    size=$(echo "$line" | cut -d',' -f6)
    nmuls=$(echo "$line" | cut -d',' -f7)
    kappa=$(echo "$line" | cut -d',' -f12 | cut -d'|' -f2)
    if [[ $count != 0 && $((count % 2)) -eq 0 ]]; then
        echo "\rowcolor{white}"
    fi
    count=$((count + 1))
    circcount=$((circcount + 1))
done < "$fname"
echo "\multirow{-$circcount}{*}{\texttt{$curname}}"
printline
