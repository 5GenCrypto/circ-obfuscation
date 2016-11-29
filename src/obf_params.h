#ifndef __AB__OBF_PARAMS_H__
#define __AB__OBF_PARAMS_H__

#include "input_chunker.h"

#include <acirc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
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
    acirc *circ;
} obf_params_t;

void
obf_params_init(obf_params_t *const p, const acirc *const circ,
                input_chunker chunker, reverse_chunker rchunker, bool simple);

int
obf_params_fwrite(const obf_params_t *const params, FILE *const fp);

int
obf_params_fread(obf_params_t *const params, FILE *const fp);

void
obf_params_clear(obf_params_t *p);

#endif
