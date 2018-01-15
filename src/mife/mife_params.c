#include "mife_params.h"
#include "mife.h"
#include "../util.h"

PRIVATE size_t
mife_params_nzs(const circ_params_t *cp)
{
    return 2 * cp->n + 1;
}

PRIVATE index_set *
mife_params_new_toplevel(const circ_params_t *const cp, size_t nzs)
{
    index_set *ix;
    size_t has_consts = cp->c ? 1 : 0;

    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    IX_Z(ix) = 1;
    for (size_t i = 0; i < cp->n - has_consts; ++i) {
        IX_W(ix, cp, i) = 1;
        IX_X(ix, cp, i) = acirc_max_var_degree(cp->circ, i);
    }
    if (has_consts) {
        IX_W(ix, cp, cp->n - 1) = 1;
        IX_X(ix, cp, cp->n - 1) = acirc_max_const_degree(cp->circ);
    }
    return ix;
}

static obf_params_t *
_new(acirc_t *circ, void *vparams)
{
    const mife_params_t *const params = vparams;
    obf_params_t *const op = calloc(1, sizeof op[0]);

    size_t has_consts = acirc_nconsts(circ) ? 1 : 0;
    circ_params_init(&op->cp, acirc_nsymbols(circ) + has_consts, circ);
    for (size_t i = 0; i < op->cp.n - has_consts; ++i) {
        op->cp.ds[i] = acirc_symlen(circ, i);
        op->cp.qs[i] = params->sigma ? acirc_symlen(circ, i) : params->base;
    }
    if (has_consts) {
        op->cp.ds[op->cp.n - 1] = acirc_nconsts(circ);
        op->cp.qs[op->cp.n - 1] = 1;
    }

    if (g_verbose)
        circ_params_print(&op->cp);

    return op;
}

static void
_free(obf_params_t *op)
{
    circ_params_clear(&op->cp);
    free(op);
}

op_vtable mife_op_vtable =
{
    .new = _new,
    .free = _free,
};
