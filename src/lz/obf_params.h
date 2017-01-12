#pragma once

#include "../input_chunker.h"
#include "../mmap.h"

#include <acirc.h>
#include <stddef.h>

struct obf_params_t {
    size_t m;       // number of secret bits
    size_t q;       // number of symbols in Σ
    size_t c;       // number of "symbolic" inputs
    size_t gamma;   // number of outputs
    size_t ell;     // bitlength of symbol
    /* size_t **types; // (gamma x (c+1))-size array */
    /* size_t M;       // max type degree in circuit over all output wires */
    /* size_t d;       // max regular degree of the circuit over all output wires */
    /* size_t D; */
    bool rachel_inputs;
    size_t npowers;
    input_chunker chunker;
    reverse_chunker rchunker;
    acirc *circ;
};
