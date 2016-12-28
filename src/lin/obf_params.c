#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

static obf_params_t *
_op_new(acirc *circ, void *vparams)
{
    const lin_obf_params_t *const params = vparams;
    obf_params_t *const p = calloc(1, sizeof(obf_params_t));

    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    if (circ->ninputs % params->symlen != 0) {
        fprintf(stderr, "error: ninputs (%lu) %% symlen (%lu) != 0\n",
                circ->ninputs, params->symlen);
        free(p);
        return NULL;
    }
    p->ell = params->symlen;
    p->c = circ->ninputs / p->ell;
    p->q = 1 << params->symlen;

    p->M = 0;
    p->types = my_calloc(p->gamma, sizeof(size_t *));
    for (size_t o = 0; o < p->gamma; o++) {
        p->types[o] = my_calloc(p->c+1, sizeof(size_t));
        type_degree(p->types[o], circ->outrefs[o], circ, p->c, chunker_in_order);
        for (size_t k = 0; k < p->c+1; k++) {
            if (p->types[o][k] > p->M) {
                p->M = p->types[o][k];
            }
        }
    }
    p->d = acirc_max_degree(circ);
    p->D = p->d + p->c + 1;
    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        /* fprintf(stderr, "* Rachel? %s\n", p->rachel_input ? "Y" : "N"); */
        fprintf(stderr, "* ℓ:      %lu\n", p->ell);
        fprintf(stderr, "* c:      %lu\n", p->c);
        fprintf(stderr, "* m:      %lu\n", p->m);
        fprintf(stderr, "* γ:      %lu\n", p->gamma);
        fprintf(stderr, "* q:      %lu\n", p->q);
        fprintf(stderr, "* M:      %lu\n", p->M);
        fprintf(stderr, "* deg:    %lu\n", p->d);
        fprintf(stderr, "* D:      %lu\n", p->D);
    }

    p->chunker  = chunker_in_order;
    p->rchunker = rchunker_in_order;
    p->circ = circ;

    return p;
}

static void
_op_free(obf_params_t *p)
{
    for (size_t i = 0; i < p->gamma; i++) {
        free(p->types[i]);
    }
    free(p->types);
    free(p);
}

static int
_op_fwrite(const obf_params_t *params, FILE *fp)
{
    (void) params; (void) fp;
    return ERR;
}

static int
_op_fread(obf_params_t *params, FILE *fp)
{
    (void) params; (void) fp;
    return ERR;
}

op_vtable lin_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
    .fwrite = _op_fwrite,
    .fread = _op_fread,
};
