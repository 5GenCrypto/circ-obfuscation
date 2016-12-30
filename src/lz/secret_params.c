#include "obf_params.h"

#include <err.h>

struct sp_info {
    obf_index *toplevel;
    const obf_params_t *op;
};
#define spinfo(x) (x)->info

static mmap_params_t
_sp_init(secret_params *sp, const obf_params_t *op, size_t kappa)
{
    mmap_params_t params;

    spinfo(sp) = my_calloc(1, sizeof(sp_info));
    spinfo(sp)->toplevel = obf_index_new_toplevel(op);
    spinfo(sp)->op = op;

    if (kappa == 0) {
        warnx("warning: need to specify kappa, defaulting to 1");
        kappa = 1;
    }
    params.kappa = kappa;
    params.nzs = spinfo(sp)->toplevel->nzs;
    params.pows = spinfo(sp)->toplevel->pows;
    params.my_pows = false;
    params.nslots = 2;

    return params;
}

static void
_sp_clear(secret_params *sp)
{
    obf_index_free(spinfo(sp)->toplevel);
    free(sp->info);
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

static sp_vtable zim_sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

static sp_vtable *
get_sp_vtable(const mmap_vtable *mmap)
{
    zim_sp_vtable.mmap = mmap;
    return &zim_sp_vtable;
}
