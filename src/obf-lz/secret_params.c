#include "obf_params.h"
#include "../circ_params.h"
#include "../index_set.h"
#include "../vtables.h"
#include "../util.h"

#include <err.h>

struct sp_info {
    index_set *toplevel;
};
#define my(x) (x)->info

static int
_sp_init(secret_params *sp, mmap_params_t *params, const obf_params_t *op,
         size_t kappa)
{
    const acirc_t *circ = op->circ;

    my(sp) = xcalloc(1, sizeof my(sp)[0]);
    if ((my(sp)->toplevel = obf_params_new_toplevel(circ, obf_params_nzs(circ))) == NULL)
        goto error;

    params->kappa = kappa ? kappa : acirc_delta(circ) + acirc_nsymbols(circ);
    params->nzs = my(sp)->toplevel->nzs;
    params->pows = my(sp)->toplevel->pows;
    params->nslots = 2;
    return OK;
error:
    index_set_free(my(sp)->toplevel);
    free(my(sp));
    return ERR;
}

static void
_sp_clear(secret_params *sp)
{
    index_set_free(my(sp)->toplevel);
    free(my(sp));
}

static const void *
_sp_toplevel(const secret_params *sp)
{
    return my(sp)->toplevel;
}

static sp_vtable _sp_vtable = {
    .mmap = NULL,
    .init = _sp_init,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
};

PRIVATE const sp_vtable *
get_sp_vtable(const mmap_vtable *mmap)
{
    _sp_vtable.mmap = mmap;
    return &_sp_vtable;
}
