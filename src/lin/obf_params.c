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
    obf_params_t *const op = calloc(1, sizeof op[0]);

    op->sigma = params->sigma;
    op->m = circ->consts.n;
    op->gamma = circ->outputs.n;
    if (circ->ninputs % params->symlen != 0) {
        fprintf(stderr, "error: ninputs (%lu) %% symlen (%lu) != 0\n",
                circ->ninputs, params->symlen);
        free(op);
        return NULL;
    }
    op->ell = params->symlen;
    op->c = circ->ninputs / op->ell;
    if (op->sigma)
        op->q = params->symlen;
    else
        op->q = 1 << params->symlen;

    op->M = 0;
    op->types = my_calloc(op->gamma, sizeof op->types[0]);
    for (size_t o = 0; o < op->gamma; o++) {
        op->types[o] = my_calloc(op->c+1, sizeof op->types[0][0]);
        type_degree(op->types[o], circ->outputs.buf[o], circ, op->c, chunker_in_order);
        for (size_t k = 0; k < op->c+1; k++) {
            if ((size_t) op->types[o][k] > op->M) {
                op->M = op->types[o][k];
            }
        }
    }
    op->d = acirc_max_degree(circ);
    /* EC:Lin16, pg. 45 */
    op->D = op->d + op->c + 1;
    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* Σ?   %s\n", op->sigma ? "Y" : "N");
        fprintf(stderr, "* ℓ:   %lu\n", op->ell);
        fprintf(stderr, "* c:   %lu\n", op->c);
        fprintf(stderr, "* m:   %lu\n", op->m);
        fprintf(stderr, "* γ:   %lu\n", op->gamma);
        fprintf(stderr, "* q:   %lu\n", op->q);
        fprintf(stderr, "* M:   %lu\n", op->M);
        fprintf(stderr, "* deg: %lu\n", op->d);
        fprintf(stderr, "* D:   %lu\n", op->D);
    }

    op->chunker  = chunker_in_order;
    op->rchunker = rchunker_in_order;
    op->circ = circ;

    return op;
}

static void
_op_free(obf_params_t *op)
{
    for (size_t i = 0; i < op->gamma; i++) {
        free(op->types[i]);
    }
    free(op->types);
    free(op);
}

op_vtable lin_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
};
