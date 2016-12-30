#include "vtables.h"
#include "obf_params.h"

#include <err.h>

struct sp_info {
    const obf_params_t *op;
    level *toplevel;
};

static mmap_params_t
_sp_init(secret_params *sp, const obf_params_t *op, size_t kappa)
{
    mmap_params_t params;
    size_t t;

    sp->info = my_calloc(1, sizeof(sp_info));
    sp->info->op = op;
    sp->info->toplevel = level_create_vzt(op);

    t = 0;
    for (size_t o = 0; o < op->gamma; o++) {
        size_t tmp = array_sum(op->types[o], op->n + 1);
        if (tmp > t)
            t = tmp;
    }
    params.kappa = kappa ? kappa : (acirc_max_total_degree(op->circ) + op->n + 1);
    params.nzs = (op->n + op->m + 1) * (op->simple ? 3 : 4);
    params.nslots = op->nslots;
    params.pows = my_calloc(params.nzs, sizeof(int));
    params.my_pows = true;
    level_flatten(params.pows, sp->info->toplevel);
    return params;
}

static void
_sp_clear(secret_params *sp)
{
    level_free(sp->info->toplevel);
    free(sp->info);
}

static const void *
_sp_toplevel(const secret_params *sp)
{
    return sp->info->toplevel;
}

static const void *
_sp_params(const secret_params *sp)
{
    return sp->info->op;
}

static sp_vtable ab_sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

const sp_vtable *
ab_get_sp_vtable(const mmap_vtable *mmap)
{
    ab_sp_vtable.mmap = mmap;
    return &ab_sp_vtable;
}
