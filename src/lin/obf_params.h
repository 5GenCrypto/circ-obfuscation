#pragma once

#include "../circ_params.h"
#include "../input_chunker.h"
#include "../mmap.h"

#include <stddef.h>

struct obf_params_t {
    circ_params_t cp;
    int **types;    // (γ x (c+1))-size array
    size_t M;       // max type degree in circuit over all output wires
    size_t d;       // max regular degree of the circuit over all output wires
    size_t D;
    int sigma;
    input_chunker chunker;
    reverse_chunker rchunker;
};

size_t
obf_params_num_encodings(const obf_params_t *const op);
