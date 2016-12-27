#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

/* TODO: set num_symbolic_inputs */

static obf_params_t *
_op_new(acirc *const circ, void *const vparams)
{
    lin_obf_params_t *const params = (lin_obf_params_t *const) vparams;
    const size_t num_symbolic_inputs = params->num_symbolic_inputs;
    obf_params_t *const p = calloc(1, sizeof(obf_params_t));

    p->rachel_input = params->rachel_input;
    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    p->types = my_calloc(p->gamma, sizeof(size_t *));

    p->ell = ceil((double) circ->ninputs / (double) num_symbolic_inputs); // length of symbols
    if (p->rachel_input)
        p->q = p->ell;
    else
        p->q = pow((double) 2, (double) p->ell); // 2^ell
    p->c = num_symbolic_inputs;

    p->M = 0;
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
    p->D = p->d + num_symbolic_inputs + 1;
    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* # inputs:  %lu\n", p->c);
        fprintf(stderr, "* # consts:  %lu\n", p->m);
        fprintf(stderr, "* # outputs: %lu\n", p->gamma);
        fprintf(stderr, "* # symbols: %lu\n", p->q);
        fprintf(stderr, "* |symbol|:  %lu\n", p->ell);
        fprintf(stderr, "* M:         %lu\n", p->M);
        fprintf(stderr, "* degree:    %lu\n", p->d);
        fprintf(stderr, "* D:         %lu\n", p->D);
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
_op_fwrite(const obf_params_t *const params, FILE *const fp)
{
    (void) params; (void) fp;
    return ERR;
}

static int
_op_fread(obf_params_t *const params, FILE *const fp)
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
