#!/usr/bin/env bash

set -e

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")

secparam=80
binary=1

script="${dir}/ggm-prf.sh"
circdir="${dir}/../circuits"
resdir="${dir}/results"

function usage {
    cat <<EOF
ggm-prf-all.sh: Collects timing information for GGM PRF circuits

usage: ggm-prf-all.sh [options]

Optional arguments:

    -s, --secparam N        Set security parameter to N (default: 80)
    -n, --non-binary        Use non-binary circuits
    -r, --results-dir DIR   Set results directory to DIR (default: results)
    -h, --help              Print this information and exit

EOF
}

while [[ $# -gt 0 ]]; do
    case "${1}" in
        -s|--secparam)
            secparam="${2}"
            shift; shift
            ;;
        -n|--non-binary)
            binary=0
            shift;
            ;;
        -r|--results-dir)
            resdir="${2}"
            shift; shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument '${1}'"
            usage
            exit 1
            ;;
    esac
done

mkdir -p "${resdir}"

# 8 bit input, 1 PRG call
for i in 32 64 128; do
    $script 1 256 ${i} "${secparam}" "${binary}" "${circdir}" "${resdir}"
done
# 8 bit input, 2 PRG calls
for i in 32 64 128; do
    $script 2 16 ${i} "${secparam}" "${binary}" "${circdir}" "${resdir}"
done
# 12 bit input, 2 PRG calls
for i in 32 64 128; do
    $script 2 64 ${i} "${secparam}" "${binary}" "${circdir}" "${resdir}"
done
# 18 bit input, 3 PRG calls
for i in 32 64 128; do
    $script 3 64 ${i} "${secparam}" "${binary}" "${circdir}" "${resdir}"
done
# 24 bit input, 4 PRG calls
for i in 32 64 128; do
    $script 4 64 ${i} "${secparam}" "${binary}" "${circdir}" "${resdir}"
done
# 28 bit input, 4 PRG calls
for i in 32 64 128; do
    $script 4 128 ${i} "${secparam}" "${binary}" "${circdir}" "${resdir}"
done
