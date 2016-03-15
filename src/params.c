#include "params.h"

#include <math.h>
#include <stdlib.h>

void params_init (params *p, circuit *circ, input_chunker chunker, size_t nsyms)
{
    p->q = nsyms;
    p->c = ceil((double) circ->ninputs / (double) nsyms);
    p->gamma = circ->noutputs;
    p->types = malloc(p->gamma * sizeof(uint32_t*));
    uint32_t max_t = 0;

    for (int i = 0; i < p->gamma; i++) {
        p->types[i] = calloc(nsyms + 1, sizeof(uint32_t));
        type_degree(p->types[i], circ->outrefs[i], circ, nsyms, chunker);

        for (int j = 0; j < nsyms; j++) {
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

void params_clear (params *p)
{
    for (int i = 0; i < p->gamma; i++) {
        free(p->types[i]);
    }
    free(p->types);
}
