#include "obf_params.h"
#include "util.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

void
obf_params_init(obf_params *p, acirc *circ, input_chunker chunker,
                reverse_chunker rchunker, size_t num_symbolic_inputs,
                bool is_rachel)
{
    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    p->types = lin_malloc(p->gamma * sizeof(size_t*));

    p->ell = ceil((double) circ->ninputs / (double) num_symbolic_inputs); // length of symbols
    if (is_rachel)
        p->q = p->ell;
    else
        p->q = pow((double) 2, (double) p->ell); // 2^ell
    if (p->q <= 0)
        errx(1, "[obf_params_init] q (size of alphabet) overflowed");
    p->c = num_symbolic_inputs;

    p->M = 0;
    for (size_t o = 0; o < p->gamma; o++) {
        p->types[o] = lin_calloc(p->c+1, sizeof(size_t));
        type_degree(p->types[o], circ->outrefs[o], circ, p->c, chunker);

        for (size_t k = 0; k < p->c+1; k++) {
            if (p->types[o][k] > p->M) {
                p->M = p->types[o][k];
            }
        }
    }
    p->d = acirc_max_degree(circ);
    p->D = p->d + num_symbolic_inputs + 1;

    p->chunker  = chunker;
    p->rchunker = rchunker;
    p->circ = circ;
}

void
obf_params_clear(obf_params *p)
{
    for (size_t i = 0; i < p->gamma; i++) {
        free(p->types[i]);
    }
    free(p->types);
}
