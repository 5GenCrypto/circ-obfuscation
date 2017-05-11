#!/usr/bin/env bash

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")

find "$dir" -name "*.ct" | xargs rm -f
find "$dir" -name "*.obf" | xargs rm -f
find "$dir" -name "*.ek" | xargs rm -f
find "$dir" -name "*.sk" | xargs rm -f

