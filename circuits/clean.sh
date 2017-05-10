#!/usr/bin/env bash

dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")

find "$dir" -name "*.ct" | xargs rm
find "$dir" -name "*.obf" | xargs rm
find "$dir" -name "*.ek" | xargs rm
find "$dir" -name "*.sk" | xargs rm

