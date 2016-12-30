#include "vtables.h"
#include "obf_params.h"

struct sp_info {
    obf_index *toplevel;
    const obf_params_t *op;
};
#define spinfo(x) (x)->info

static mmap_params_t
_sp_init(secret_params *sp, const obf_params_t *op, size_t kappa)
{
    mmap_params_t params;

    spinfo(sp) = calloc(1, sizeof(sp_info));
    spinfo(sp)->toplevel = obf_index_create_toplevel(op->circ);
    spinfo(sp)->op = op;

    params.kappa = kappa ? kappa : (acirc_delta(op->circ) + op->circ->ninputs);
    params.nzs = spinfo(sp)->toplevel->nzs;
    params.nslots = 2;
    params.pows = (int *) spinfo(sp)->toplevel->pows;
    params.my_pows = false;
    return params;
}

static void
_sp_clear(secret_params *sp)
{
    obf_index_destroy(spinfo(sp)->toplevel);
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

sp_vtable *
zim_get_sp_vtable(const mmap_vtable *mmap)
{
    zim_sp_vtable.mmap = mmap;
    return &zim_sp_vtable;
}
