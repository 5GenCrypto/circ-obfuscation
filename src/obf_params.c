#include "obf_params.h"

#include <math.h>
#include <stdlib.h>

void obf_params_init (obf_params *p, circuit *circ, input_chunker chunker, size_t nsyms)
{
    p->m = circ->nconsts;
    p->q = nsyms;
    p->c = ceil((double) circ->ninputs / (double) nsyms);
    p->gamma = circ->noutputs;
    p->types = malloc(p->gamma * sizeof(uint32_t*));
    p->M = 0;

    for (int i = 0; i < p->gamma; i++) {
        p->types[i] = calloc(p->q+1, sizeof(uint32_t));
        type_degree(p->types[i], circ->outrefs[i], circ, nsyms, chunker);

        for (int j = 0; j < p->q; j++) {
            if (p->types[i][j] > p->M) {
                p->M = p->types[i][j];
            }
        }
    }
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
    rop->gamma = op->gamma;
    rop->types = malloc(rop->gamma * sizeof(uint32_t*));
    for (int i = 0; i < rop->gamma; i++) {
        rop->types[i] = malloc((rop->q+1) * sizeof(uint32_t));
        for (int j = 0; j < rop->q+1; j++) {
            rop->types[i][j] = op->types[i][j];
        }
    }
}
