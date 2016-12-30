#include "obf_params.h"
#include "../util.h"

struct sp_info {
    const obf_params_t *op;
    level *toplevel;
};
#define spinfo(x) (x)->info

static mmap_params_t
_sp_init(secret_params *sp, const obf_params_t *op, size_t kappa)
{
    mmap_params_t params;
    size_t t;

    spinfo(sp) = my_calloc(1, sizeof(sp_info));
    spinfo(sp)->op = op;
    spinfo(sp)->toplevel = level_create_vzt(op);
    if (g_verbose) {
        fprintf(stderr, "toplevel: ");
        level_fprint(stderr, spinfo(sp)->toplevel);
    }

    t = 0;
    for (size_t o = 0; o < op->gamma; o++) {
        size_t tmp = array_sum(op->types[o], op->c+1);
        if (tmp > t)
            t = tmp;
    }
    params.kappa = kappa ? kappa : (t + op->D);
    params.nzs = (op->q+1) * (op->c+2) + op->gamma;
    params.pows = my_calloc(params.nzs, sizeof(int));
    level_flatten((int *) params.pows, spinfo(sp)->toplevel);
    params.my_pows = true;
    params.nslots = op->c + 3;

    return params;
}

static void
_sp_clear(secret_params *sp)
{
    if (spinfo(sp)->toplevel)
        level_free(spinfo(sp)->toplevel);
    if (spinfo(sp))
        free(spinfo(sp));
}

static const void *
_sp_toplevel(const secret_params *sp)
{
    return spinfo(sp)->toplevel;
}

static const void *
_sp_params(const secret_params *sp)
{
    return spinfo(sp)->op;
}

static sp_vtable lin_sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

static const sp_vtable *
get_sp_vtable(const mmap_vtable *mmap)
{
    lin_sp_vtable.mmap = mmap;
    return &lin_sp_vtable;
}

