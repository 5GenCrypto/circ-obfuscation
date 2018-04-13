#!/usr/bin/env bash

set -e

if [[ $# < 5 || $# > 7 ]]; then
    echo "Usage: ggm-prf.sh <nprgs> <symlen> <keylen> <secparam> <binary> [circuit-dir] [results-dir]"
    exit 1
fi

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")

nprgs=${1}
symlen=${2}
keylen=${3}
secparam=${4}
binary=${5}
circdir=${6}
resdir=${7}

inplen=$(python -c "import math; print('%d' % (math.log(${symlen}, 2) * ${nprgs},))")
input=$(python -c "print('0' * ${nprgs} * ${symlen})")

if [[ "${circdir}" == "" ]]; then
    circdir=$(readlink -f "${dir}/../circuits")
fi
if [[ "${resdir}" == "" ]]; then
    resdir=$(readlink -f "${dir}/results")
fi
mkdir -p "${resdir}"/"${secparam}"
mio=$(readlink -f "${dir}/../mio.sh")

if [[ "${binary}" == "1" ]]; then
    ext="acirc2"
else
    ext="acirc"
fi
circuit="ggm_sigma_${nprgs}_${symlen}_${keylen}.${ext}"

cp "${circdir}"/"${circuit}" /tmp/"${circuit}"

args="--verbose --mmap CLT --scheme CMR /tmp/${circuit}"

${mio} obf obfuscate --secparam "${secparam}" ${args} 2>&1 | tee /tmp/obfuscate.txt
ngates=$(grep "# gates" /tmp/obfuscate.txt | cut -d' ' -f5)
nencodings=$(grep "# encodings" /tmp/obfuscate.txt | cut -d' ' -f4)
kappa=$(grep "κ:" /tmp/obfuscate.txt | head -1 | tr -s ' ' | cut -d' ' -f3)
obf_time=$(grep "Total:" /tmp/obfuscate.txt | cut -d' ' -f2)
obf_size=$(ls -lh "/tmp/${circuit}.obf" | cut -d' ' -f5)
obf_mem=$(grep "Memory" /tmp/obfuscate.txt | tr -s ' ' | cut -d' ' -f2)

${mio} obf evaluate ${args} "${input}" 2>&1 | tee /tmp/evaluate.txt
eval_time=$(grep "Total" /tmp/evaluate.txt | cut -d' ' -f2)
eval_mem=$(grep "Memory" /tmp/evaluate.txt | tr -s ' ' | cut -d' ' -f2)

rm "/tmp/${circuit}.obf"

cat <<EOF | tee "${resdir}"/"${secparam}"/"${circuit}".txt

* circuit: ...... ${circuit}

* n: ............ ${inplen}
* k: ............ ${keylen}
* |Σ|: .......... ${symlen}
* # prgs: ....... ${nprgs}
* # gates: ...... ${ngates}
* # encodings: .. ${nencodings}
* κ: ............ ${kappa}

* Obf time: ..... ${obf_time}
* Obf size: ..... ${obf_size}
* Obf mem: ...... ${obf_mem}
* Eval time: .... ${eval_time}
* Eval mem: ..... ${eval_mem}

EOF
