#include "obf_index.h"
#include "obf_params.h"
#include "vtables.h"
#include "../util.h"

#include <err.h>

struct sp_info {
    obf_index *toplevel;
    const obf_params_t *op;
};
#define spinfo(x) (x)->info

static int
_sp_init(secret_params *sp, mmap_params_t *params, const obf_params_t *op,
         size_t kappa)
{
    spinfo(sp) = my_calloc(1, sizeof(sp_info));
    spinfo(sp)->toplevel = obf_index_new_toplevel(op);
    spinfo(sp)->op = op;

    params->kappa = kappa ? kappa : acirc_delta(op->circ) + op->circ->ninputs;
    params->nzs = spinfo(sp)->toplevel->nzs;
    for (size_t i = 0; i < params->nzs; ++i) {
        if ((int) spinfo(sp)->toplevel->pows[i] < 0) {
            fprintf(stderr, "error: toplevel overflow\n");
            obf_index_free(spinfo(sp)->toplevel);
            free(spinfo(sp));
            return ERR;
        }
    }
    params->pows = spinfo(sp)->toplevel->pows;
    params->my_pows = false;
    params->nslots = 2;

    return OK;
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

static sp_vtable _sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
    .params = _sp_params,
};

PRIVATE const sp_vtable *
get_sp_vtable(const mmap_vtable *mmap)
{
    _sp_vtable.mmap = mmap;
    return &_sp_vtable;
}
