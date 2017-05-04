#include "obf_params.h"
#include "level.h"
#include "vtables.h"
#include "../mmap.h"
#include "../util.h"

struct pp_info {
    const obf_params_t *op;
    level *toplevel;
    bool my_toplevel;
};
#define info(x) (x)->info

static int
_pp_init(const sp_vtable *vt, public_params *pp, const secret_params *sp)
{
    pp->info = calloc(1, sizeof pp->info[0]);
    pp->info->toplevel = vt->toplevel(sp);
    pp->info->my_toplevel = false;
    pp->info->op = vt->params(sp);
    return OK;
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
    pp->info = calloc(1, sizeof pp->info[0]);
    pp->info->toplevel = level_create_vzt(&op->cp, op->M, op->D);
    pp->info->my_toplevel = true;
    pp->info->op = op;
    return OK;
}

static void
_pp_clear(public_params *pp)
{
    if (pp->info->my_toplevel)
        level_free(pp->info->toplevel);
    free(pp->info);
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
