#include "obf_params.h"
#include "vtables.h"
#include "../util.h"

struct pp_info {
    const obf_params_t *op;
    index_set *toplevel;
    bool local;
};
#define ppinfo(x) x->info

static void
_pp_init(const sp_vtable *vt, public_params *pp, const secret_params *sp)
{
    pp->info = calloc(1, sizeof pp->info[0]);
    pp->info->toplevel = vt->toplevel(sp);
    pp->info->op = vt->params(sp);
    pp->info->local = false;
}

static void
_pp_clear(public_params *pp)
{
    if (pp->info->local)
        index_set_free(pp->info->toplevel);
    free(pp->info);
}

static void
_pp_fwrite(const public_params *pp, FILE *fp)
{
    (void) pp; (void) fp;
}

static void
_pp_fread(public_params *pp, const obf_params_t *op, FILE *fp)
{
    (void) fp;
    pp->info = calloc(1, sizeof pp->info[0]);
    pp->info->toplevel = obf_params_new_toplevel(&op->cp, obf_params_nzs(&op->cp));
    pp->info->op = op;
    pp->info->local = true;
}

static const void *
_pp_params(const public_params *pp)
{
    return pp->info->op;
}

static const void *
_pp_toplevel(const public_params *pp)
{
    return pp->info->toplevel;
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
