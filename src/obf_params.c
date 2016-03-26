#include "obf_params.h"

#include <math.h>
#include <stdlib.h>

void obf_params_init (
    obf_params *p,
    circuit *circ,
    input_chunker chunker,
    reverse_chunker rchunker,
    size_t num_symbolic_inputs
) {
    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    p->types = malloc(p->gamma * sizeof(uint32_t*));

    p->ell = ceil((double) circ->ninputs / (double) num_symbolic_inputs); // length of symbols
    p->q = 1 << p->ell; // 2^ell
    p->c = num_symbolic_inputs;

    p->M = 0;
    for (int i = 0; i < p->gamma; i++) {
        p->types[i] = calloc(p->c+1, sizeof(uint32_t));
        type_degree(p->types[i], circ->outrefs[i], circ, p->c, chunker);

        for (int j = 0; j < p->c+1; j++) {
            if (p->types[i][j] > p->M) {
                p->M = p->types[i][j];
            }
        }
    }

    p->chunker  = chunker;
    p->rchunker = rchunker;
    p->circ = circ;
}

void obf_params_clear (obf_params *p)
{
    for (int i = 0; i < p->gamma; i++) {
        free(p->types[i]);
    }
    free(p->types);
}

void obf_params_init_set (obf_params *rop, const obf_params *op)
{
    rop->m = op->m;
    rop->q = op->q;
    rop->c = op->c;
    rop->M = op->M;
    rop->ell = op->ell;
    rop->gamma = op->gamma;
    rop->chunker  = op->chunker;
    rop->rchunker = op->rchunker;
    rop->types = malloc(rop->gamma * sizeof(uint32_t*));
    for (int i = 0; i < rop->gamma; i++) {
        rop->types[i] = malloc((rop->q+1) * sizeof(uint32_t));
        for (int j = 0; j < rop->q+1; j++) {
            rop->types[i][j] = op->types[i][j];
        }
    }
}
