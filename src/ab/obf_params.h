#ifndef __AB__OBF_PARAMS_H__
#define __AB__OBF_PARAMS_H__

#include <acirc.h>
#include <stdbool.h>
#include <stddef.h>

struct obf_params_t {
    size_t m;                   /* number of secret bits */
    size_t n;                   /* input length */
    size_t gamma;               /* output length */
    size_t M;                   /* max type degree */
    size_t d;                   /* max regular degree of circuit */
    size_t D;                   /* d + n */
    size_t nslots;
    size_t **types;             /* (γ × (m+1))-size array */
    bool simple;
    input_chunker chunker;
    reverse_chunker rchunker;
    const acirc *circ;
};

#endif
