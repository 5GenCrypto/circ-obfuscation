#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <acirc.h>
#include <assert.h>

PRIVATE size_t
obf_params_nzs(const circ_params_t *cp)
{
    const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0;
    const size_t ninputs = cp->nslots - has_consts;
    const size_t q = array_max(cp->qs, ninputs);
    return (2 + q) * ninputs + has_consts;
}

PRIVATE index_set *
obf_params_new_toplevel(const circ_params_t *cp, size_t nzs)
{
    index_set *ix;
    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    ix_y_set(ix, cp, acirc_max_const_degree(cp->circ));
    for (size_t k = 0; k < acirc_nsymbols(cp->circ); k++) {
        for (size_t s = 0; s < cp->qs[k]; s++)
            ix_s_set(ix, cp, k, s, acirc_max_var_degree(cp->circ, k));
        ix_z_set(ix, cp, k, 1);
        ix_w_set(ix, cp, k, 1);
    }
    return ix;
}

PRIVATE size_t
obf_params_num_encodings(const obf_params_t *op)
{
    const circ_params_t *cp = &op->cp;
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);
    size_t sum = nconsts + op->npowers + noutputs;
    for (size_t i = 0; i < acirc_nsymbols(cp->circ); ++i) {
        sum += cp->qs[i] * cp->ds[i];
        sum += cp->qs[i] * op->npowers;
        sum += cp->qs[i] * noutputs * 2;
    }
    return sum;
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
_new(acirc_t *circ, void *vparams)
{
    (void) circ;
    const lz_obf_params_t *params = vparams;
    obf_params_t *op;

    if ((op = calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->npowers = params->npowers;
    return op;
}

static void
_print(const obf_params_t *op)
{
    fprintf(stderr, "Obfuscation parameters:\n");
    fprintf(stderr, "* # powers: .. %lu\n", op->npowers);
    fprintf(stderr, "* # encodings: %lu\n", obf_params_num_encodings(op));
}

static int
_fwrite(const obf_params_t *op, FILE *fp)
{
    circ_params_fwrite(&op->cp, fp);
    int_fwrite(op->sigma, fp);
    size_t_fwrite(op->npowers, fp);
    return OK;
}

static obf_params_t *
_fread(acirc_t *circ, FILE *fp)
{
    obf_params_t *op;

    if ((op = calloc(1, sizeof op[0])) == NULL)
        return NULL;
    circ_params_fread(&op->cp, circ, fp);
    int_fread(&op->sigma, fp);
    size_t_fread(&op->npowers, fp);
    return op;
}

op_vtable lz_op_vtable =
{
    .new = _new,
    .free = _free,
    .fwrite = _fwrite,
    .fread = _fread,
    .print = _print,
};
