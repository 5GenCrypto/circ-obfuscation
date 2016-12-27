#include "vtables.h"

struct pp_info {
    obf_params_t *op;
    level *toplevel;
};
#define info(x) (x)->info

static void
_pp_init(const sp_vtable *const vt, public_params *const pp,
           const secret_params *const sp)
{
    pp->info = calloc(1, sizeof(pp_info));
    pp->info->toplevel = vt->toplevel(sp);
    pp->info->op = vt->params(sp);
}

static void
_pp_fwrite(const public_params *const pp, FILE *const fp)
{
    level_fwrite(pp->info->toplevel, fp);
}

static void
_pp_fread(public_params *const pp, const obf_params_t *const op,
          FILE *const fp)
{
    pp->info = calloc(1, sizeof(pp_info));
    pp->info->toplevel = level_create_vzt(op);
    level_fread(pp->info->toplevel, fp);
    pp->info->op = op;
}

static void
_pp_clear(public_params *const pp)
{
    free(pp->info);
}

static void *
_pp_params(const public_params *const pp)
{
    return pp->info->op;
}

static void *
_pp_toplevel(const public_params *const pp)
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

const pp_vtable *
lin_get_pp_vtable(const mmap_vtable *const mmap)
{
    _pp_vtable.mmap = mmap;
    return &_pp_vtable;
}
