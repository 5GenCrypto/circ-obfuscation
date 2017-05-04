#!/usr/bin/env bash

get_name () {
    echo "$1" | cut -d',' -f1
}
get_mode () {
    echo "$1" | cut -d',' -f2
}
get_ninputs () {
    echo "$1" | cut -d',' -f3
}
get_nconsts () {
    echo "$1" | cut -d',' -f4
}
get_nouts () {
    echo "$1" | cut -d',' -f5
}
get_size  () {
    echo "$1" | cut -d"," -f6
}
get_nmuls () {
    echo "$1" | cut -d"," -f7
}
get_depth () {
    echo "$1" | cut -d"," -f8
}
get_degree () {
    echo "$1" | cut -d"," -f9
}
get_nencodings_lin () {
    echo "$1" | cut -d"," -f10
}
get_nencodings_lz () {
    echo "$1" | cut -d"," -f11
}
get_nencodings_mife () {
    echo "$1" | cut -d"," -f12
}
get_kappa_lin () {
    echo "$1" | cut -d"," -f13
}
get_kappa_lz () {
    echo "$1" | cut -d"," -f14
}
get_kappa_mife () {
    echo "$1" | cut -d"," -f15
}
