#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

PRIVATE size_t
obf_params_num_encodings(const obf_params_t *const op)
{
    const circ_params_t *cp = &op->cp;
    const size_t nconsts = cp->circ->consts.n;
    const size_t has_consts = nconsts ? 1 : 0;
    const size_t ninputs = cp->n - has_consts;
    const size_t noutputs = cp->m;
    const size_t q = array_max(cp->qs, ninputs);
    const size_t d = array_max(cp->ds, ninputs);

    return q * ninputs + q * d * ninputs + nconsts + 2 * q * ninputs * noutputs \
        + 4 * noutputs + 1;
}

static obf_params_t *
_op_new(acirc *circ, void *vparams)
{
    const lin_obf_params_t *const params = vparams;
    obf_params_t *const op = calloc(1, sizeof op[0]);

    if (circ->ninputs % params->symlen != 0) {
        fprintf(stderr, "error: ninputs (%lu) %% symlen (%lu) != 0\n",
                circ->ninputs, params->symlen);
        free(op);
        return NULL;
    }
    const size_t has_consts = circ->consts.n ? 1 : 0;
    circ_params_init(&op->cp, circ->ninputs / params->symlen + has_consts, circ);
    const size_t nconsts = op->cp.circ->consts.n;
    const size_t ninputs = op->cp.n - has_consts;
    const size_t noutputs = op->cp.m;
    op->sigma = params->sigma;
    for (size_t i = 0; i < ninputs; ++i) {
        op->cp.ds[i] = params->symlen;
        if (op->sigma)
            op->cp.qs[i] = params->symlen;
        else
            op->cp.qs[i] = 1 << params->symlen;
    }
    if (has_consts) {
        op->cp.ds[ninputs] = nconsts;
        op->cp.qs[ninputs] = 1;
    }

    op->M = 0;
    op->types = my_calloc(noutputs, sizeof op->types[0]);
    for (size_t o = 0; o < noutputs; o++) {
        op->types[o] = my_calloc(ninputs + 1, sizeof op->types[0][0]);
        type_degree(op->types[o], circ->outputs.buf[o], circ, ninputs, chunker_in_order);
        for (size_t k = 0; k < ninputs + 1; k++) {
            if ((size_t) op->types[o][k] > op->M) {
                op->M = op->types[o][k];
            }
        }
    }
    op->d = acirc_max_degree(circ);
    /* EC:Lin16, pg. 45 */
    op->D = op->d + ninputs + 1;
    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* Σ: .... %s\n", op->sigma ? "Yes" : "No");
        fprintf(stderr, "* n: ......... %lu\n", op->cp.n);
        for (size_t i = 0; i < op->cp.n; ++i) {
            fprintf(stderr, "*   %lu: ....... %lu (%lu)\n", i + 1, op->cp.ds[i], op->cp.qs[i]);
        }
        fprintf(stderr, "* m: ......... %lu\n", op->cp.m);
        fprintf(stderr, "* M: .... %lu\n", op->M);
        fprintf(stderr, "* d: .... %lu\n", op->d);
        fprintf(stderr, "* D: .... %lu\n", op->D);
        fprintf(stderr, "* # encodings: %lu\n", obf_params_num_encodings(op));
    }

    op->chunker  = chunker_in_order;
    op->rchunker = rchunker_in_order;

    return op;
}

static void
_op_free(obf_params_t *op)
{
    for (size_t i = 0; i < op->cp.m; i++) {
        free(op->types[i]);
    }
    free(op->types);
    circ_params_clear(&op->cp);
    free(op);
}

op_vtable lin_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
};
