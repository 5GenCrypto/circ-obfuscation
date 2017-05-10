#include "obf_params.h"
#include "../mife/mife.h"
#include "obfuscator.h"
#include "util.h"

size_t
mobf_num_encodings(const obf_params_t *op)
{
    const circ_params_t *cp = &op->cp;
    size_t count = 0;
    for (size_t i = 0; i < cp->n; ++i)
        count += cp->qs[i] * mife_num_encodings_encrypt(cp, i);
    return count;
}

static void
_free(obf_params_t *op)
{
    if (op) {
        circ_params_clear(&op->cp);
        free(op);
    }
}

static obf_params_t *
_new(acirc *circ, void *vparams)
{
    const mobf_obf_params_t *const params = vparams;
    obf_params_t *const op = my_calloc(1, sizeof op[0]);

    if (circ->ninputs % params->symlen != 0) {
        fprintf(stderr, "error: ninputs (%lu) %% symlen (%lu) != 0\n",
                circ->ninputs, params->symlen);
        _free(op);
        return NULL;
    }
    const size_t consts = circ->consts.n ? 1 : 0;
    circ_params_init(&op->cp, circ->ninputs / params->symlen + consts, circ);
    for (size_t i = 0; i < op->cp.n - consts; ++i) {
        op->cp.ds[i] = params->symlen;
        op->cp.qs[i] = params->sigma ? params->symlen : params->base;
    }
    if (consts) {
        op->cp.ds[op->cp.n - 1] = circ->consts.n;
        op->cp.qs[op->cp.n - 1] = 1;
    }
    op->sigma = params->sigma;
    op->npowers = params->npowers;
    op->chunker  = chunker_in_order;
    op->rchunker = rchunker_in_order;

    if (g_verbose) {
        circ_params_print(&op->cp);
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* Σ: ......... %s\n", op->sigma ? "Yes" : "No");
        fprintf(stderr, "* # powers: .. %lu\n", op->npowers);
        fprintf(stderr, "* # encodings: %lu\n", mobf_num_encodings(op));
    }

    return op;
}

static int
_fwrite(const obf_params_t *op, FILE *fp)
{
    circ_params_fwrite(&op->cp, fp);
    fprintf(fp, "%d\n", op->sigma);
    fprintf(fp, "%lu\n", op->npowers);
    return OK;
}

static obf_params_t *
_fread(acirc *circ, FILE *fp)
{
    obf_params_t *op;

    op = my_calloc(1, sizeof op[0]);
    circ_params_fread(&op->cp, circ, fp);
    fscanf(fp, "%d\n", &op->sigma);
    fscanf(fp, "%lu\n", &op->npowers);
    op->chunker = chunker_in_order;
    op->rchunker = rchunker_in_order;
    return op;
}

op_vtable mobf_op_vtable =
{
    .new = _new,
    .free = _free,
    .fwrite = _fwrite,
    .fread = _fread,
};
