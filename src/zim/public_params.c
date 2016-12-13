#include "public_params.h"
#include "secret_params.h"

#include "obf_index.h"
#include "vtables.h"

struct pp_info {
    const obf_params_t *op;
    obf_index *toplevel;
    bool local;
};
#define my(x) x->info

static void
_pp_init(const sp_vtable *const vt, public_params *const pp,
           const secret_params *const sp)
{
    pp->info = calloc(1, sizeof(pp_info));
    pp->info->toplevel = vt->toplevel(sp);
    pp->info->op = zim_sp_op(sp);
    pp->info->local = false;
}

static void
_pp_clear(public_params *const pp)
{
    free(pp->info);
}

static void
_pp_fwrite(const public_params *const pp, FILE *const fp)
{
    obf_index_write(fp, my(pp)->toplevel);
}

static void
_pp_fread(public_params *const pp, const obf_params_t *const op,
          FILE *const fp)
{
    pp->info = calloc(1, sizeof(pp_info));
    pp->info->toplevel = obf_index_read(fp);
    pp->info->op = op;
    pp->info->local = true;
}

static void *
_pp_params(const public_params *const pp)
{
    (void) pp;
    return NULL;
}

static void *
_pp_toplevel(const public_params *const pp)
{
    return pp->info->toplevel;
}

static pp_vtable zim_pp_vtable = {
    .mmap = NULL,
    .init = _pp_init,
    .clear = _pp_clear,
    .fwrite = _pp_fwrite,
    .fread = _pp_fread,
    .toplevel = _pp_toplevel,
    .params = _pp_params,
};

pp_vtable *
zim_get_pp_vtable(const mmap_vtable *const mmap)
{
    zim_pp_vtable.mmap = mmap;
    return &zim_pp_vtable;
}

const obf_params_t *
zim_pp_op(const public_params *const pp)
{
    return my(pp)->op;
}
