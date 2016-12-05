#include "vtables.h"
#include "level.h"

struct pp_info {
    obf_params_t *op;
    level *toplevel;
};

static void
ab_pp_init(const sp_vtable *const vt, public_params *const pp,
           const secret_params *const sp)
{
    pp->info = calloc(1, sizeof(pp_info));
    pp->info->toplevel = vt->toplevel(sp);
    pp->info->op = vt->params(sp);
}

static void
ab_pp_fwrite(const public_params *const pp, FILE *const fp)
{
    level_fwrite(pp->info->toplevel, fp);
}

static void
ab_pp_fread(public_params *const pp, const obf_params_t *const op,
            FILE *const fp)
{
    pp->info = calloc(1, sizeof(pp_info));
    pp->info->toplevel = level_create_vzt(op);
    level_fread(pp->info->toplevel, fp);
    pp->info->op = op;
}

static void
ab_pp_clear(public_params *const pp)
{
    free(pp->info);
}

static void *
ab_pp_params(const public_params *const pp)
{
    return pp->info->op;
}

static void *
ab_pp_toplevel(const public_params *const pp)
{
    return pp->info->toplevel;
}

static pp_vtable ab_pp_vtable = {
    .mmap = NULL,
    .init = ab_pp_init,
    .clear = ab_pp_clear,
    .fwrite = ab_pp_fwrite,
    .fread = ab_pp_fread,
    .toplevel = ab_pp_toplevel,
    .params = ab_pp_params,
};

const pp_vtable *
ab_get_pp_vtable(const mmap_vtable *const mmap)
{
    ab_pp_vtable.mmap = mmap;
    return &ab_pp_vtable;
}
