#include "mife_params.h"
#include "mife.h"
#include "../util.h"

PRIVATE size_t
mife_params_nzs(const circ_params_t *cp)
{
    return 2 * cp->nslots + 1;
}

PRIVATE index_set *
mife_params_new_toplevel(const circ_params_t *cp, size_t nzs)
{
    index_set *ix;
    size_t has_consts = acirc_nconsts(cp->circ) + acirc_nsecrets(cp->circ) ? 1 : 0;

    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    IX_Z(ix) = 1;
    for (size_t i = 0; i < cp->nslots - has_consts; ++i) {
        IX_W(ix, cp, i) = 1;
        IX_X(ix, cp, i) = acirc_max_var_degree(cp->circ, i);
    }
    if (has_consts) {
        IX_W(ix, cp, cp->nslots - 1) = 1;
        IX_X(ix, cp, cp->nslots - 1) = acirc_max_const_degree(cp->circ);
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
