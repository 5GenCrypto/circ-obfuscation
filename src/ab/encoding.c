#include "vtables.h"
#include "../util.h"

#include <assert.h>

struct encoding_info {
    level *lvl;
};

static int
_encoding_new(const pp_vtable *vt, encoding *enc,
                const public_params *pp)
{
    enc->info = calloc(1, sizeof(encoding_info));
    enc->info->lvl = level_new((const obf_params_t *) vt->params(pp));
    return 0;
}

static void
_encoding_free(encoding *enc)
{
    if (enc->info) {
        if (enc->info->lvl) {
            level_free(enc->info->lvl);
        }
        free(enc->info);
    }
}

static int
_encoding_print(const encoding *enc)
{
    fprintf(stderr, "Encoding: ");
    level_fprint(stderr, enc->info->lvl);
    return 0;
}

static int *
_encode(encoding *rop, const void *set)
{
    int *pows;
    const level *lvl = (const level *const) set;
    
    level_set(rop->info->lvl, lvl);
    pows = calloc(lvl->nrows * lvl->ncols, sizeof(int));
    level_flatten(pows, lvl);
    return pows;
}

static int
_encoding_set(encoding *rop, const encoding *x)
{
    level_set(rop->info->lvl, x->info->lvl);
    return 0;
}

static int
_encoding_mul(const pp_vtable *vt, encoding *rop,
              const encoding *x, const encoding *y,
              const public_params *pp)
{
    (void) vt; (void) pp;
    level_add(rop->info->lvl, x->info->lvl, y->info->lvl);
    return OK;
}

static int
_encoding_add(const pp_vtable *vt, encoding *rop,
              const encoding *x, const encoding *y,
              const public_params *pp)
{
    (void) vt; (void) pp;
    if (!level_eq(x->info->lvl, y->info->lvl)) {
        fprintf(stderr, "[%s] unequal levels\nx=\n", __func__);
        level_fprint(stderr, x->info->lvl);
        fprintf(stderr, "y=\n");
        level_fprint(stderr, y->info->lvl);
        return ERR;
    }
    level_set(rop->info->lvl, x->info->lvl);
    return OK;
}

static int
_encoding_sub(const pp_vtable *vt, encoding *rop,
              const encoding *x, const encoding *y,
              const public_params *pp)
{
    (void) vt; (void) pp;
    if (!level_eq(x->info->lvl, y->info->lvl)) {
        fprintf(stderr, "[%s] unequal levels\nx=\n", __func__);
        level_fprint(stderr, x->info->lvl);
        fprintf(stderr, "y=\n");
        level_fprint(stderr, y->info->lvl);
        return ERR;
    }
    level_set(rop->info->lvl, x->info->lvl);
    return OK;
}
    
static int
_encoding_is_zero(const pp_vtable *vt, const encoding *x,
                  const public_params *pp)
{
    if (!level_eq(x->info->lvl, vt->toplevel(pp))) {
        fprintf(stderr, "[%s] unequal levels\n", __func__);
        fprintf(stderr, "this level:\n");
        level_fprint(stderr, x->info->lvl);
        fprintf(stderr, "top level:\n");
        level_fprint(stderr, vt->toplevel(pp));
        return ERR;
    }
    return OK;
}

static void
_encoding_fread(encoding *x, FILE *fp)
{
    x->info = calloc(1, sizeof(encoding_info));
    x->info->lvl = calloc(1, sizeof(level));
    level_fread(x->info->lvl, fp);
    GET_NEWLINE(fp);
}

static void
_encoding_fwrite(const encoding *x, FILE *fp)
{
    level_fwrite(x->info->lvl, fp);
    PUT_NEWLINE(fp);
}

static encoding_vtable ab_encoding_vtable =
{
    .mmap = NULL,
    .new = _encoding_new,
    .free = _encoding_free,
    .print = _encoding_print,
    .encode = _encode,
    .set = _encoding_set,
    .mul = _encoding_mul,
    .add = _encoding_add,
    .sub = _encoding_sub,
    .is_zero = _encoding_is_zero,
    .fread = _encoding_fread,
    .fwrite = _encoding_fwrite,
    .mmap_set = NULL,
};

const encoding_vtable *
ab_get_encoding_vtable(const mmap_vtable *mmap)
{
    ab_encoding_vtable.mmap = mmap;
    return &ab_encoding_vtable;
}
