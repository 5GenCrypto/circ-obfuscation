#pragma once

#include "../circ_params.h"
#include "../index_set.h"
#include "../mmap.h"

#include <acirc.h>
#include <stddef.h>

struct obf_params_t {
    const acirc_t *circ;
    int sigma;
    size_t npowers;
};

static inline void
ix_y_set(index_set *ix, const acirc_t *circ, int pow)
{
    if (acirc_nconsts(circ))
        ix->pows[0] = pow;
}
static inline int
ix_y_get(index_set *ix, const acirc_t *circ)
{
    if (acirc_nconsts(circ))
        return ix->pows[0];
    else
        return 0;
}
static inline void
ix_s_set(index_set *ix, const acirc_t *circ, size_t k, size_t s, int pow)
{
    const size_t has_consts = acirc_nconsts(circ) ? 1 : 0;
    size_t n = 0;
    for (size_t i = 0; i < k; ++i)
        n += acirc_symnum(circ, i) + 2;
    ix->pows[has_consts + n + s] = pow;
}
static inline int
ix_s_get(index_set *ix, const acirc_t *circ, size_t k, size_t s)
{
    const size_t has_consts = acirc_nconsts(circ) ? 1 : 0;
    size_t n = 0;
    for (size_t i = 0; i < k; ++i)
        n += acirc_symnum(circ, i) + 2;
    return ix->pows[has_consts + n + s];
}
static inline void
ix_z_set(index_set *ix, const acirc_t *circ, size_t k, int pow)
{
    const size_t has_consts = acirc_nconsts(circ) ? 1 : 0;
    size_t n = 0;
    for (size_t i = 0; i < k; ++i)
        n += acirc_symnum(circ, i) + 2;
    n += acirc_symnum(circ, k);
    ix->pows[has_consts + n] = pow;
}
static inline void
ix_w_set(index_set *ix, const acirc_t *circ, size_t k, int pow)
{
    const size_t has_consts = acirc_nconsts(circ) ? 1 : 0;
    size_t n = 0;
    for (size_t i = 0; i < k; ++i)
        n += acirc_symnum(circ, i) + 2;
    n += acirc_symnum(circ, k);
    ix->pows[has_consts + n + 1] = pow;
}

const acirc_t * pp_circ(const public_params *pp);
size_t obf_params_nzs(const acirc_t *circ);
index_set * obf_params_new_toplevel(const acirc_t *circ, size_t nzs);
size_t obf_params_num_encodings(const obf_params_t *op);
