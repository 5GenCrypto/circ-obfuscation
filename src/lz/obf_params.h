#ifndef __ZIM__OBF_PARAMS_H__
#define __ZIM__OBF_PARAMS_H__

#include <acirc.h>
#include <stddef.h>

struct obf_params_t {
    size_t m;       // number of secret bits
    size_t q;       // number of symbols in sigma = 2^\ell for binary, \ell for rachel vectors
    size_t c;       // number of "symbolic" inputs
    size_t gamma;   // number of outputs
    size_t ell;     // length of symbols
    size_t **types; // (gamma x (c+1))-size array
    size_t M;       // max type degree in circuit over all output wires
    size_t d;       // max regular degree of the circuit over all output wires
    size_t D;
    input_chunker chunker;
    reverse_chunker rchunker;
    acirc *circ;
};

#endif
