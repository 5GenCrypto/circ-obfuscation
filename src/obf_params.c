#include "obf_params.h"

#include <math.h>
#include <stdlib.h>

void obf_params_init (obf_params *p, circuit *circ, input_chunker chunker, size_t nsyms)
{
    p->q = nsyms;
    p->c = ceil((double) circ->ninputs / (double) nsyms);
    p->gamma = circ->noutputs;
    p->types = malloc(p->gamma * sizeof(uint32_t*));
    uint32_t max_t = 0;

    for (int i = 0; i < p->gamma; i++) {
        p->types[i] = calloc(p->q, sizeof(uint32_t));
        type_degree(p->types[i], circ->outrefs[i], circ, nsyms, chunker);

        for (int j = 0; j < p->q; j++) {
            if (p->types[i][j] > max_t)
                max_t = p->types[i][j];
        }
    }
    p->m = max_t;

    uint32_t max_d = 0;
    for (int i = 0; i < p->gamma; i++) {
        uint32_t d = degree(circ, circ->outrefs[i]);
        if (d > max_d)
            max_d = d;
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
    rop->q = op->q;
    rop->c = op->c;
    rop->gamma = op->gamma;
    rop->types = malloc(rop->gamma * sizeof(uint32_t*));
    for (int i = 0; i < rop->gamma; i++) {
        rop->types[i] = calloc(rop->q, sizeof(uint32_t));
        for (int j = 0; j < rop->q; j++) {
            rop->types[i][j] = op->types[i][j];
        }
    }
    rop->m = op->m;
}
