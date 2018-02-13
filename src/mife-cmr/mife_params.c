#include "mife_params.h"
#include "mife.h"
#include "../util.h"

PRIVATE size_t
mife_params_nzs(const acirc_t *circ)
{
    return 2 * acirc_nslots(circ) + 1;
}

PRIVATE index_set *
mife_params_new_toplevel(const acirc_t *circ)
{
    const size_t nzs = mife_params_nzs(circ);
    index_set *ix;

    ix = index_set_new(nzs);
    IX_Z(ix) = 1;
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
        IX_W(ix, circ, i) = 1;
        IX_X(ix, circ, i) = acirc_max_var_degree(circ, i);
    }
    for (size_t i = acirc_nsymbols(circ); i < acirc_nslots(circ); ++i) {
        IX_W(ix, circ, i) = 1;
        IX_X(ix, circ, i) = acirc_max_const_degree(circ);
    }
    return ix;
}

static obf_params_t *
mife_cmr_op_new(const acirc_t *circ, void *vparams)
{
    (void) circ;
    mife_cmr_params_t *params = vparams;
    obf_params_t *op;

    op = xcalloc(1, sizeof op[0]);
    op->npowers = params->npowers;
    return op;
}

static void
mife_cmr_op_free(obf_params_t *op)
{
    free(op);
}

static void
mife_cmr_op_print(const obf_params_t *op, const acirc_t *circ)
{
    fprintf(stderr, "MIFE parameters:\n");
    fprintf(stderr, "———— # powers: ................... %lu\n", op->npowers);
    fprintf(stderr, "———— # encodings (setup): ........ %lu\n",
            mife_num_encodings_setup(circ, op->npowers));
    for (size_t i = 0; i < acirc_nslots(circ); ++i)
        fprintf(stderr, "———— # encodings (encrypt slot %lu): %lu\n",
                i, mife_num_encodings_encrypt(circ, i));
}

op_vtable mife_cmr_op_vtable =
{
    .new = mife_cmr_op_new,
    .free = mife_cmr_op_free,
    .print = mife_cmr_op_print,
};
