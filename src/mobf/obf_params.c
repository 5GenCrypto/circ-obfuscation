#include "obf_params.h"
#include "obfuscator.h"
#include "util.h"

static size_t
num_encodings(const obf_params_t *op)
{
    const circ_params_t *cp = &op->cp;
    size_t count = 0;
    count += 1 + cp->m;
    for (size_t i = 0; i < cp->n; ++i)
        count += cp->qs[i] * (cp->ds[i] + op->npowers + cp->m);
    return count;
}

static obf_params_t *
_new(acirc *circ, void *vparams)
{
    const mobf_obf_params_t *const params = vparams;
    obf_params_t *const op = calloc(1, sizeof op[0]);

    if (circ->ninputs % params->symlen != 0) {
        fprintf(stderr, "error: ninputs (%lu) %% symlen (%lu) != 0\n",
                circ->ninputs, params->symlen);
        free(op);
        return NULL;
    }
    const size_t consts = circ->consts.n ? 1 : 0;
    circ_params_init(&op->cp, circ->ninputs / params->symlen + consts, circ);
    op->sigma = params->sigma;
    op->npowers = params->npowers;
    for (size_t i = 0; i < op->cp.n - consts; ++i) {
        op->cp.ds[i] = params->symlen;
        if (op->sigma)
            op->cp.qs[i] = params->symlen;
        else
            op->cp.qs[i] = 1 << params->symlen;
    }
    if (consts) {
        op->cp.ds[op->cp.n - 1] = circ->consts.n;
        op->cp.qs[op->cp.n - 1] = 1;
    }

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
        fprintf(stderr, "* # encodings: %lu\n", num_encodings(op));
    }

    op->chunker  = chunker_in_order;
    op->rchunker = rchunker_in_order;

    return op;
}

static void
_free(obf_params_t *op)
{
    circ_params_clear(&op->cp);
    free(op);
}

op_vtable mobf_op_vtable =
{
    .new = _new,
    .free = _free
};
