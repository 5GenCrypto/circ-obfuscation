#include "obf_params.h"
#include "../mmap.h"
#include "../vtables.h"

#include "../circ_params.h"
#include "../index_set.h"
#include "../util.h"

struct sp_info {
    const acirc_t *circ;
    index_set *toplevel;
    mmap_polylog_switch_params *sparams;
};
#define my(x) (x)->info

static int
_sp_init(secret_params *sp, mmap_params_t *mp, const obf_params_t *op, size_t kappa)
{
    (void) kappa;
    const acirc_t *circ = op->circ;

    if ((my(sp) = calloc(1, sizeof my(sp)[0])) == NULL)
        return ERR;
    my(sp)->toplevel = obf_params_new_toplevel(circ, obf_params_nzs(circ));
    my(sp)->circ = circ;

    mp->kappa = 0;
    mp->nzs = my(sp)->toplevel->nzs;
    mp->pows = my(sp)->toplevel->pows;
    mp->nslots = 1 + acirc_ninputs(circ) + 1;
    return OK;
}

static int
_sp_fwrite(const secret_params *sp, FILE *fp)
{
    (void) sp; (void) fp;
    return OK;
}

static int
_sp_fread(secret_params *sp, const obf_params_t *op, FILE *fp)
{
    (void) fp;
    if ((my(sp) = calloc(1, sizeof my(sp)[0])) == NULL)
        return ERR;
    my(sp)->toplevel = obf_params_new_toplevel(op->circ, obf_params_nzs(op->circ));
    my(sp)->circ = op->circ;
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
    .fwrite = _sp_fwrite,
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
