#!/usr/bin/env bash

set -e

fname=${1:-kappas.csv}
source "$(dirname "$0")/utils.sh"

circcount=0
count=0
curname=

printline () {
    echo " && $mode && \num{$size} & \num{$nmuls} & \num{$degree} \tabularnewline"
}

while read -r input; do
    line=$(echo "$input" | tr -d ' ')
    name=$(get_name "$line")
    if [[ $name != aes1r && $name != aes1r_64_1 ]]; then
        continue
    fi
    name=$(perl -e "\$line = \"$name\"; \$line =~ s/_/\\\_/g; print \$line")
    if [[ $curname != "" ]]; then
        if [[ $name != "$curname" ]]; then
            echo "\multirow{-$circcount}{*}{\texttt{$curname}}"
        fi
        printline
    fi
    mode=$(get_mode "$line")
    if [[ $mode != "dsl" && ! $mode =~ ^o(1|2|3) ]]; then
        continue
    fi
    case $mode in
        dsl )
            mode="-O0" ;;
        o1 )
            mode="-O1" ;;
        o2 )
            mode="-O2" ;;
        o3 )
            mode="-O3" ;;
    esac
    if [[ $name != "$curname" ]]; then
        if [[ $curname != "" ]]; then
            echo "\midrule"
        fi
        circcount=0
        curname=$name
    fi
    size=$(get_size "$line")
    nmuls=$(get_nmuls "$line")
    degree=$(get_degree "$line")
    if [[ $count != 0 && $((count % 2)) -eq 0 ]]; then
        echo "\rowcolor{white}"
    fi
    count=$((count + 1))
    circcount=$((circcount + 1))
done < "$fname"
echo "\multirow{-$circcount}{*}{\texttt{$curname}}"
printline
