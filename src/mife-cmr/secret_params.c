#include "../mmap.h"
#include "../vtables.h"

#include "../circ_params.h"
#include "../index_set.h"
#include "mife_params.h"
#include "../util.h"

struct sp_info {
    index_set *toplevel;
};
#define my(x) (x)->info

static int
_sp_init(secret_params *sp, mmap_params_t *mp, const obf_params_t *op, size_t kappa)
{
    const acirc_t *circ = op->circ;
    const size_t delta = acirc_delta(circ);
    if (delta == 0)
        return ERR;
    if ((my(sp) = calloc(1, sizeof my(sp)[0])) == NULL)
        return ERR;
    my(sp)->toplevel = mife_params_new_toplevel(circ);

    mp->kappa = kappa ? kappa : (size_t) max(delta + 1, acirc_nsymbols(circ));
    mp->nzs = my(sp)->toplevel->nzs;
    mp->pows = my(sp)->toplevel->pows;
    mp->nslots = 1 + acirc_nslots(circ);
    return OK;
}

static int
_sp_fread(secret_params *sp, const obf_params_t *op, FILE *fp)
{
    (void) fp;
    const acirc_t *circ = op->circ;
    if ((my(sp) = calloc(1, sizeof my(sp)[0])) == NULL)
        return ERR;
    my(sp)->toplevel = mife_params_new_toplevel(circ);
    return OK;
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
    .fread = _sp_fread,
    .clear = _sp_clear,
    .toplevel = _sp_toplevel,
};

PRIVATE const sp_vtable *
get_sp_vtable(const mmap_vtable *mmap)
{
    _sp_vtable.mmap = mmap;
    return &_sp_vtable;
}
