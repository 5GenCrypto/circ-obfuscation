#!/usr/bin/env bash

set -e

if [[ $# != 1 && $# != 2 ]]; then
    echo "Usage: ggm-prf-all.sh <secparam> [results-dir]"
    exit 1
fi

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")

script="${dir}/ggm-prf.sh"
circdir="${dir}/../circuits"

secparam=${1}
resdir=${2}
if [[ ${resdir} == "" ]]; then
    resdir="${dir}/results"
fi
mkdir -p "${resdir}"

for i in 32 64 128; do
    $script 1 256 ${i} "${secparam}" "${circdir}" "${resdir}"
done
for i in 32 64 128; do
    $script 2 16 ${i} "${secparam}" "${circdir}" "${resdir}"
done
for i in 32 64 128; do
    $script 2 64 ${i} "${secparam}" "${circdir}" "${resdir}"
done
