#!/usr/bin/env bash

set -e

dir=${1:-.}

scriptdir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
"$scriptdir"/circuit-info.sh > "$dir"/table-circuits-data.tex
"$scriptdir"/compiler-info.sh > "$dir"/table-compilers-data.tex
"$scriptdir"/kappa-info-ggm.sh > "$dir"/table-kappas-ggm-data.tex
"$scriptdir"/kappa-info.sh > "$dir"/table-kappas-data.tex
"$scriptdir"/optimizer-info.sh > "$dir"/table-optimizer-data.tex

