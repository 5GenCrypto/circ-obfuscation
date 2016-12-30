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

    /* op->M = 0; */
    /* op->types = my_calloc(op->gamma, sizeof(size_t *)); */
    /* for (size_t o = 0; o < op->gamma; o++) { */
    /*     op->types[o] = my_calloc(op->c+1, sizeof(size_t)); */
    /*     type_degree(op->types[o], circ->outrefs[o], circ, op->c, chunker_in_order); */
    /*     for (size_t k = 0; k < op->c+1; k++) { */
    /*         if (op->types[o][k] > op->M) { */
    /*             op->M = op->types[o][k]; */
    /*         } */
    /*     } */
    /* } */
    /* op->d = acirc_max_degree(circ); */
    /* op->D = op->d + op->c + 1; */

    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* ℓ:        %lu\n", op->ell);
        fprintf(stderr, "* c:        %lu\n", op->c);
        fprintf(stderr, "* m:        %lu\n", op->m);
        fprintf(stderr, "* γ:        %lu\n", op->gamma);
        fprintf(stderr, "* q:        %lu\n", op->q);
        /* fprintf(stderr, "* M:        %lu\n", op->M); */
        /* fprintf(stderr, "* d:        %lu\n", op->d); */
        /* fprintf(stderr, "* D:        %lu\n", op->D); */
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