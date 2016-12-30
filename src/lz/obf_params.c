#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <assert.h>

static obf_params_t *
_op_new(acirc *circ, void *vparams)
{
    const lz_obf_params_t *const params = vparams;
    obf_params_t *const op = calloc(1, sizeof(obf_params_t));

    op->rachel_inputs = params->rachel_inputs;
    op->m = circ->nconsts;
    op->gamma = circ->noutputs;
    if (circ->ninputs % params->symlen != 0) {
        fprintf(stderr, "error: ninputs (%lu) %% symlen (%lu) != 0\n",
                circ->ninputs, params->symlen);
        free(op);
        return NULL;
    }
    op->ell = params->symlen;
    op->c = circ->ninputs / op->ell;
    if (op->rachel_inputs)
        op->q = params->symlen;
    else
        op->q = 1 << params->symlen;
    op->npowers = params->npowers;

    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* ℓ:        %lu\n", op->ell);
        fprintf(stderr, "* c:        %lu\n", op->c);
        fprintf(stderr, "* m:        %lu\n", op->m);
        fprintf(stderr, "* γ:        %lu\n", op->gamma);
        fprintf(stderr, "* q:        %lu\n", op->q);
        fprintf(stderr, "* # powers: %lu\n", op->npowers);
    }

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

static int
_op_fwrite(const obf_params_t *op, FILE *fp)
{
    (void) op; (void) fp;
    return ERR;
}

static int
_op_fread(obf_params_t *op, FILE *fp)
{
    (void) op; (void) fp;
    return ERR;
}

op_vtable lz_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
    .fwrite = _op_fwrite,
    .fread = _op_fread,
};
