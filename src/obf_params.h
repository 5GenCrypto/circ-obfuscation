#ifndef __SRC_OBF_PARAMS_H__
#define __SRC_OBF_PARAMS_H__

#include "circuit.h"
#include "input_chunker.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t m;       // number of secret bits
    size_t q;       // number of symbols in sigma = 2^\ell
    size_t c;       // number of "symbolic" inputs
    size_t gamma;   // number of outputs
    size_t ell;     // length of symbols
    size_t **types; // (gamma x (c+1))-size array
    size_t M;       // max type degree in circuit over all output wires
    size_t d;       // max regular degree of the circuit over all output wires
    size_t D;
    input_chunker chunker;
    reverse_chunker rchunker;
    circuit *circ;
} obf_params;

void obf_params_init (
    obf_params *p,
    circuit *circ,
    input_chunker chunker,
    reverse_chunker rchunker,
    size_t num_symbolic_inputs
);

void obf_params_clear (obf_params *p);

#endif
