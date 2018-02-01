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

    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
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
_new(acirc_t *circ, void *vparams)
{
    (void) circ;
    mife_cmr_params_t *params = vparams;
    obf_params_t *op;

    if ((op = my_calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->npowers = params->npowers;
    return op;
}

static void
_free(obf_params_t *op)
{
    circ_params_clear(&op->cp);
    free(op);
}

op_vtable mife_cmr_op_vtable =
{
    .new = _new,
    .free = _free,
};
