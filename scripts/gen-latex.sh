#!/usr/bin/env bash

usage () {
    echo "gen-latex.sh: Generates info for latex tables"
    echo ""
    echo "Flags:"
    echo "  -d, --dir D  Place produced files in D (default: .)"
    echo "  -k, --kappa  Generate kappas.csv file"
    echo "  -h, --help   Print this info and exit"
    exit "$1"
}

_=$(getopt -o d:kh --long dir:,kappa,help)
if [ $? != 0 ]; then
    echo "Error: failed parsing options"
    usage 1
fi

dir='.'
kappa=0

while true; do
    case "$1" in
        -d | --dir )
            case "$2" in
                "") shift 2 ;;
                *) dir=$2; shift 2 ;;
            esac ;;
        -k | --kappa )
            kappa=1; shift ;;
        -h | --help )
            usage 0 ;;
        -- ) shift; break ;;
        *) break ;;
    esac
done

set -ex

scriptdir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
if [[ $kappa -eq 1 ]]; then
    "$scriptdir"/get-kappas.sh | tee "$scriptdir"/kappas.csv
fi
"$scriptdir"/circuit-info.sh > "$dir"/table-circuits-data.tex
"$scriptdir"/compiler-info.sh > "$dir"/table-compilers-data.tex
"$scriptdir"/aes1r-compiler-info.sh > "$dir"/table-aes1r-compiler-data.tex
"$scriptdir"/kappa-info-ggm.sh > "$dir"/table-kappas-ggm-data.tex
"$scriptdir"/kappa-info.sh > "$dir"/table-kappas-data.tex
"$scriptdir"/optimizer-info.sh > "$dir"/table-optimizer-data.tex
"$scriptdir"/prg-info.sh > "$dir"/table-prgs-data.tex
