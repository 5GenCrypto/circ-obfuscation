#include "mife_params.h"

size_t
mife_params_nzs(const circ_params_t *cp)
{
    return 2 * cp->n + 1;
}

index_set *
mife_params_new_toplevel(const circ_params_t *const cp, size_t nzs)
{
    index_set *ix;
    size_t consts = cp->circ->consts.n ? 1 : 0;

    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    IX_Z(ix) = 1;
    for (size_t i = 0; i < cp->n - consts; ++i) {
        IX_W(ix, cp, i) = 1;
        IX_X(ix, cp, i) = acirc_max_var_degree(cp->circ, i);
    }
    if (consts) {
        IX_W(ix, cp, cp->n - 1) = 1;
        IX_X(ix, cp, cp->n - 1) = acirc_max_const_degree(cp->circ);
    }
    return ix;
}

size_t
mife_params_num_encodings_setup(const circ_params_t *cp)
{
    return 1 + cp->m;
}

size_t
mife_params_num_encodings_encrypt(const circ_params_t *cp, size_t slot, size_t npowers)
{
    return cp->ds[slot] + npowers + cp->m;
}
