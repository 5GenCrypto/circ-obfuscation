#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <acirc.h>
#include <assert.h>

PRIVATE size_t
obf_params_nzs(const acirc_t *circ)
{
    const size_t has_consts = acirc_nconsts(circ) ? 1 : 0;
    size_t n = 0;
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        n += acirc_symnum(circ, i) + 2;
    return n + has_consts;
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
    const size_t nconsts = acirc_nconsts(circ);
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
obf_lz_op_free(obf_params_t *op)
{
    if (op) {
        free(op);
    }
}

static obf_params_t *
obf_lz_op_new(const acirc_t *circ, void *vparams)
{
    const obf_lz_params_t *params = vparams;
    obf_params_t *op;

    if ((op = calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->circ = circ;
    op->npowers = params->npowers;
    return op;
}

static void
obf_lz_op_print(const obf_params_t *op)
{
    fprintf(stderr, "Obfuscation parameters:\n");
    fprintf(stderr, "———— # powers: .. %lu\n", op->npowers);
    fprintf(stderr, "———— # encodings: %lu\n", obf_params_num_encodings(op));
}

op_vtable obf_lz_op_vtable =
{
    .new = obf_lz_op_new,
    .free = obf_lz_op_free,
    .print = obf_lz_op_print,
};
