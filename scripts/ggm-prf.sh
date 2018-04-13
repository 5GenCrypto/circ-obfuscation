#!/usr/bin/env bash

set -e

if [[ $# != 4 && $# != 5 ]]; then
    echo "Usage: ggm-prf.sh <nprgs> <symlen> <keylen> <secparam> [circuit-dir]"
    exit 1
fi

nprgs=$1
symlen=$2
keylen=$3
secparam=$4
circuits=$5

inplen=$(python -c "import math; print('%d' % (math.log(${symlen}, 2) * ${nprgs},))")
eval=$(python -c "print('0' * ${nprgs} * ${symlen})")

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
if [[ $circuits == "" ]]; then
    circuits=$(readlink -f "${dir}/../circuits")
fi
mio=$(readlink -f "${dir}/../mio.sh")


circuit="ggm_sigma_${nprgs}_${symlen}_${keylen}.acirc2"

cp circuits/"${circuit}" /tmp/"${circuit}"

args="--verbose --mmap CLT --scheme CMR /tmp/${circuit}"

$mio obf obfuscate --secparam "${secparam}" $args 2>&1 | tee /tmp/obfuscate.txt
ngates=$(grep "# gates" /tmp/obfuscate.txt | cut -d' ' -f5)
nencodings=$(grep "# encodings" /tmp/obfuscate.txt | cut -d' ' -f4)
kappa=$(grep "κ:" /tmp/obfuscate.txt | head -1 | tr -s ' ' | cut -d' ' -f3)
obf_time=$(grep "Total:" /tmp/obfuscate.txt | cut -d' ' -f2)
obf_size=$(ls -lh "/tmp/${circuit}.obf" | cut -d' ' -f5)
obf_mem=$(grep "Memory" /tmp/obfuscate.txt | tr -s ' ' | cut -d' ' -f2)
$mio obf evaluate $args $eval 2>&1 | tee /tmp/evaluate.txt
eval_time=$(grep "Total" /tmp/evaluate.txt | cut -d' ' -f2)
eval_mem=$(grep "Memory" /tmp/evaluate.txt | tr -s ' ' | cut -d' ' -f2)
rm "/tmp/${circuit}.obf"

cat <<EOF | tee -a results.txt

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
