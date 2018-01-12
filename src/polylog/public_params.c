#include "obf_params.h"
#include "../circ_params.h"
#include "../index_set.h"
#include "../vtables.h"
#include "../util.h"

struct pp_info {
    const circ_params_t *cp;
    index_set *toplevel;
    bool local;
};
#define my(pp) pp->info

static int
_pp_init(const sp_vtable *vt, public_params *pp, const secret_params *sp)
{
    if ((my(pp) = calloc(1, sizeof my(pp)[0])) == NULL)
        return ERR;
    my(pp)->toplevel = vt->toplevel(sp);
    my(pp)->cp = vt->params(sp);
    my(pp)->local = false;
    return OK;
}

static void
_pp_clear(public_params *pp)
{
    if (my(pp)->local)
        index_set_free(my(pp)->toplevel);
    free(my(pp));
}

static int
_pp_fwrite(const public_params *pp, FILE *fp)
{
    (void) pp; (void) fp;
    return OK;
}

static int
_pp_fread(public_params *pp, const obf_params_t *op, FILE *fp)
{
    (void) fp;
    const circ_params_t *cp = &op->cp;
    if ((my(pp) = calloc(1, sizeof my(pp)[0])) == NULL)
        return ERR;
    my(pp)->toplevel = obf_params_new_toplevel(cp, obf_params_nzs(cp));
    my(pp)->cp = cp;
    my(pp)->local = true;
    return OK;
}

static const void *
_pp_params(const public_params *pp)
{
    return my(pp)->cp;
}

static const void *
_pp_toplevel(const public_params *pp)
{
    return my(pp)->toplevel;
}

static pp_vtable _pp_vtable = {
    .mmap = NULL,
    .init = _pp_init,
    .clear = _pp_clear,
    .fwrite = _pp_fwrite,
    .fread = _pp_fread,
    .toplevel = _pp_toplevel,
    .params = _pp_params,
};

PRIVATE const pp_vtable *
get_pp_vtable(const mmap_vtable *mmap)
{
    _pp_vtable.mmap = mmap;
    return &_pp_vtable;
}
