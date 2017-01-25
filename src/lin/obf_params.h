#pragma once

#include <acirc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../input_chunker.h"

struct obf_params_t {
    size_t m;       // number of secret bits
    size_t q;       // number of symbols in Σ = 2^ℓ for binary, ℓ for Σ vectors
    size_t c;       // number of "symbolic" inputs
    size_t gamma;   // number of outputs
    size_t ell;     // length of symbols
    int **types;    // (γ x (c+1))-size array
    size_t M;       // max type degree in circuit over all output wires
    size_t d;       // max regular degree of the circuit over all output wires
    size_t D;
    bool sigma;
    input_chunker chunker;
    reverse_chunker rchunker;
    acirc *circ;
};
