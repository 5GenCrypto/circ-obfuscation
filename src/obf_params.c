#include "obf_params.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

void
obf_params_init(obf_params *p, acirc *circ, input_chunker chunker,
                reverse_chunker rchunker, bool simple)
{
    p->n = circ->ninputs;
    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    p->M = 0;
    p->types = lin_calloc(p->gamma, sizeof(size_t *));
    for (size_t o = 0; o < p->gamma; o++) {
        p->types[o] = lin_calloc(p->n + p->m + 1, sizeof(size_t));
        type_degree(p->types[o], circ->outrefs[o], circ, p->n + p->m, chunker);
        for (size_t k = 0; k < p->n + p->m; k++) {
            printf("%lu ", p->types[o][k]);
            if (p->types[o][k] > p->M) {
                p->M = p->types[o][k];
            }
        }
        printf("\n");
    }
    p->d = acirc_max_degree(circ);
    p->D = p->d + p->n;
    printf("%lu %lu\n", p->d, p->D);
    p->nslots = simple ? 2 : (p->n + 2);

    p->circ = circ;
    p->chunker = chunker;
    p->rchunker = rchunker;
    p->simple = simple;
}

void
obf_params_clear(obf_params *p)
{
    for (size_t i = 0; i < p->gamma; i++) {
        free(p->types[i]);
    }
    free(p->types);
}
