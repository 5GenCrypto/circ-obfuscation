#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <assert.h>

PRIVATE size_t
num_encodings(const obf_params_t *op)
{
    return op->c * op->q * op->ell
        + op->c * op->q * op->npowers
        + op->c * op->q * op->gamma
        + op->c * op->q * op->gamma
        + op->m
        + op->npowers
        + op->gamma;
}

static obf_params_t *
_op_new(acirc *circ, void *vparams)
{
    const lz_obf_params_t *const params = vparams;
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
    op->npowers = params->npowers;

    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* Σ: %s\n", op->sigma ? "Y" : "N");
        fprintf(stderr, "* ℓ: %lu\n", op->ell);
        fprintf(stderr, "* c: %lu\n", op->c);
        fprintf(stderr, "* m: %lu\n", op->m);
        fprintf(stderr, "* γ: %lu\n", op->gamma);
        fprintf(stderr, "* q: %lu\n", op->q);
        fprintf(stderr, "* # powers: %lu\n", op->npowers);
        fprintf(stderr, "* # encodings: %lu\n", num_encodings(op));
    }

    assert(op->ell > 0);

    op->chunker  = chunker_in_order;
    op->rchunker = rchunker_in_order;
    op->circ = circ;

    return op;
}

static void
_op_free(obf_params_t *op)
{
    free(op);
}

op_vtable lz_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
};
