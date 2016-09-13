#ifndef __SRC_OBF_PARAMS_H__
#define __SRC_OBF_PARAMS_H__

#include "input_chunker.h"
#include <acirc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    size_t m;                   /* number of secret bits */
    size_t n;                   /* input length */
    size_t gamma;               /* output length */
    size_t **types;             /* (γ × (m+1))-size array */
    size_t M;                   /* max type degree */
    size_t d;                   /* max regular degree of circuit */
    size_t D;
    size_t nslots;
    bool simple;
    input_chunker chunker;
    reverse_chunker rchunker;
    acirc *circ;
} obf_params;

void
obf_params_init(obf_params *p, acirc *circ, input_chunker chunker,
                reverse_chunker rchunker, bool simple);
void
obf_params_clear(obf_params *p);

#endif
