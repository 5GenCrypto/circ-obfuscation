#pragma once

#include "../circ_params.h"
#include "../index_set.h"
#include "../input_chunker.h"
#include "../mmap.h"

#include <acirc.h>
#include <stddef.h>

/* struct obf_params_t { */
/*     circ_params_t cp; */
/*     int sigma; */
/*     size_t npowers; */
/* }; */

static inline void
ix_y_set(index_set *ix, const circ_params_t *cp, int pow)
{
    if (acirc_nconsts(cp->circ))
        ix->pows[0] = pow;
}
static inline int
ix_y_get(index_set *ix, const circ_params_t *cp)
{
    if (acirc_nconsts(cp->circ))
        return ix->pows[0];
    else
        return 0;
}
static inline void
ix_s_set(index_set *ix, const circ_params_t *cp, size_t k, size_t s, int pow)
{
    const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0;
    ix->pows[has_consts + cp->qs[k] * k + s] = pow;
}
static inline int
ix_s_get(index_set *ix, const circ_params_t *cp, size_t k, size_t s)
{
    const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0;
    return ix->pows[has_consts + cp->qs[k] * k + s];
}
static inline void
ix_z_set(index_set *ix, const circ_params_t *cp, size_t k, int pow)
{
    const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0;
    ix->pows[has_consts + cp->qs[k] * (cp->n - has_consts) + k] = pow;
}
static inline void
ix_w_set(index_set *ix, const circ_params_t *cp, size_t k, int pow)
{
    const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0;
    ix->pows[has_consts + (1 + cp->qs[k]) * (cp->n - has_consts) + k] = pow;
}

size_t obf_params_nzs(const circ_params_t *cp);
index_set * obf_params_new_toplevel(const circ_params_t *cp, size_t nzs);
size_t obf_params_num_encodings(const obf_params_t *op);
