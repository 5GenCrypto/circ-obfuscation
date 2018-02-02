#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <acirc.h>
#include <assert.h>

PRIVATE size_t
obf_params_nzs(const acirc_t *circ)
{
    const size_t has_consts = acirc_nconsts(circ) + acirc_nsecrets(circ) ? 1 : 0;
    const size_t ninputs = acirc_nsymbols(circ);
    size_t q = 0;
    for (size_t i = 0; i < ninputs; ++i)
        q = q > acirc_symnum(circ, i) ? q : acirc_symnum(circ, i);
    return (2 + q) * ninputs + has_consts;
}

PRIVATE index_set *
obf_params_new_toplevel(const acirc_t *circ, size_t nzs)
{
    index_set *ix;
    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    ix_y_set(ix, circ, acirc_max_const_degree(circ));
    for (size_t k = 0; k < acirc_nsymbols(circ); k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++)
            ix_s_set(ix, circ, k, s, acirc_max_var_degree(circ, k));
        ix_z_set(ix, circ, k, 1);
        ix_w_set(ix, circ, k, 1);
    }
    return ix;
}

PRIVATE size_t
obf_params_num_encodings(const obf_params_t *op)
{
    const acirc_t *circ = op->circ;
    const size_t nconsts = acirc_nconsts(circ) + acirc_nsecrets(circ);
    const size_t noutputs = acirc_noutputs(circ);
    size_t sum = nconsts + op->npowers + noutputs;
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
        sum += acirc_symnum(circ, i) * acirc_symlen(circ, i);
        sum += acirc_symnum(circ, i) * op->npowers;
        sum += acirc_symnum(circ, i) * noutputs * 2;
    }
    return sum;
}

static void
_free(obf_params_t *op)
{
    if (op) {
        free(op);
    }
}

static obf_params_t *
_new(acirc_t *circ, void *vparams)
{
    const lz_obf_params_t *params = vparams;
    obf_params_t *op;

    if ((op = calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->circ = circ;
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
    op->circ = circ;
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
