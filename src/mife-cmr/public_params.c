#include "../circ_params.h"
#include "../index_set.h"
#include "mife_params.h"
#include "../vtables.h"
#include "../util.h"

struct pp_info {
    const acirc_t *circ;
    index_set *toplevel;
    bool local;
};
#define my(pp) pp->info

const acirc_t *
pp_circ(const public_params *pp)
{
    return my(pp)->circ;
}

static int
_pp_init(const sp_vtable *vt, public_params *pp, const secret_params *sp,
         const obf_params_t *op)
{
    my(pp) = xcalloc(1, sizeof my(pp)[0]);
    my(pp)->toplevel = (index_set *) vt->toplevel(sp);
    my(pp)->circ = op->circ;
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
_pp_fread(public_params *pp, const acirc_t *circ, FILE *fp)
{
    (void) fp;
    my(pp) = xcalloc(1, sizeof my(pp)[0]);
    my(pp)->toplevel = mife_params_new_toplevel(circ);
    my(pp)->circ = circ;
    my(pp)->local = true;
    return OK;
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
};

PRIVATE const pp_vtable *
get_pp_vtable(const mmap_vtable *mmap)
{
    _pp_vtable.mmap = mmap;
    return &_pp_vtable;
}
