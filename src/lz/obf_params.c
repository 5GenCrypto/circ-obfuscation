#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <assert.h>

PRIVATE size_t
obf_params_nzs(const circ_params_t *cp)
{
    const size_t ninputs = cp->n - 1;
    size_t q = array_max(cp->qs, ninputs);
    return (2 + q) * ninputs + 1;
}

PRIVATE index_set *
obf_params_new_toplevel(const circ_params_t *cp, size_t nzs)
{
    index_set *ix;
    const size_t ninputs = cp->n - 1;
    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    IX_Y(ix) = acirc_max_const_degree(cp->circ);
    for (size_t k = 0; k < ninputs; k++) {
        for (size_t s = 0; s < cp->qs[k]; s++) {
            IX_S(ix, cp, k, s) = acirc_max_var_degree(cp->circ, k);
        }
        IX_Z(ix, cp, k) = 1;
        IX_W(ix, cp, k) = 1;
    }
    return ix;
}


PRIVATE size_t
obf_params_num_encodings(const obf_params_t *op)
{
    const circ_params_t *cp = &op->cp;
    const size_t ninputs = cp->n - 1;
    const size_t nconsts = cp->ds[ninputs];
    const size_t noutputs = cp->m;
    size_t sum = nconsts + op->npowers + noutputs;
    for (size_t i = 0; i < ninputs; ++i) {
        sum += cp->qs[i] * cp->ds[i];
        sum += cp->qs[i] * op->npowers;
        sum += cp->qs[i] * noutputs * 2;
    }
    return sum;
}

static obf_params_t *
_op_new(acirc *circ, void *vparams)
{
    const lz_obf_params_t *const params = vparams;
    obf_params_t *const op = calloc(1, sizeof op[0]);

    if (circ->ninputs % params->symlen != 0) {
        fprintf(stderr, "error: ninputs (%lu) %% symlen (%lu) != 0\n",
                circ->ninputs, params->symlen);
        free(op);
        return NULL;
    }
    circ_params_init(&op->cp, circ->ninputs / params->symlen + 1, circ);
    op->sigma = params->sigma;
    op->npowers = params->npowers;
    for (size_t i = 0; i < op->cp.n - 1; ++i) {
        op->cp.ds[i] = params->symlen;
        if (op->sigma)
            op->cp.qs[i] = params->symlen;
        else
            op->cp.qs[i] = 1 << params->symlen;
    }
    op->cp.ds[op->cp.n - 1] = circ->consts.n;
    op->cp.qs[op->cp.n - 1] = 1;

    if (g_verbose) {
        circ_params_print(&op->cp);
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* Σ: ......... %s\n", op->sigma ? "Yes" : "No");
        fprintf(stderr, "* n: ......... %lu\n", op->cp.n);
        for (size_t i = 0; i < op->cp.n; ++i) {
            fprintf(stderr, "*   %lu: ....... %lu (%lu)\n", i + 1, op->cp.ds[i], op->cp.qs[i]);
        }
        fprintf(stderr, "* m: ......... %lu\n", op->cp.m);
        fprintf(stderr, "* # powers: .. %lu\n", op->npowers);
        fprintf(stderr, "* # encodings: %lu\n", obf_params_num_encodings(op));
    }

    op->chunker  = chunker_in_order;
    op->rchunker = rchunker_in_order;

    return op;
}

static void
_op_free(obf_params_t *op)
{
    circ_params_clear(&op->cp);
    free(op);
}

op_vtable lz_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
};
