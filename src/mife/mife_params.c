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
    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    IX_Z(ix) = 1;
    for (size_t i = 0; i < cp->n; ++i) {
        IX_W(ix, cp, i) = 1;
        if (i >= cp->circ->ninputs) {
            /* these are constants */
            IX_X(ix, cp, i) = acirc_max_const_degree(cp->circ);
        } else {
            IX_X(ix, cp, i) = acirc_max_var_degree(cp->circ, i);
        }
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
