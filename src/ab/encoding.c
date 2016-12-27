#include "vtables.h"
#include "level.h"
#include "util.h"

#include <assert.h>

struct encoding_info {
    level *lvl;
};

static int
_encoding_new(const pp_vtable *const vt, encoding *const enc,
                const public_params *const pp)
{
    enc->info = calloc(1, sizeof(encoding_info));
    enc->info->lvl = level_new((obf_params_t *) vt->params(pp));
    return 0;
}

static void
_encoding_free(encoding *const enc)
{
    if (enc->info) {
        if (enc->info->lvl) {
            level_free(enc->info->lvl);
        }
        free(enc->info);
    }
}

static int
_encoding_print(const encoding *const enc)
{
    printf("Encoding:\n");
    level_print(enc->info->lvl);
    return 0;
}

static int *
_encode(encoding *const rop, const void *const set)
{
    int *pows;
    const level *const lvl = (const level *const) set;
    
    level_set(rop->info->lvl, lvl);
    pows = calloc(lvl->nrows * lvl->ncols, sizeof(int));
    level_flatten(pows, lvl);
    return pows;
}

static int
_encoding_set(encoding *const rop, const encoding *const x)
{
    level_set(rop->info->lvl, x->info->lvl);
    return 0;
}

static int
_encoding_mul(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp;
    level_add(rop->info->lvl, x->info->lvl, y->info->lvl);
    return 0;
}

static int
_encoding_add(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp;
    assert(level_eq(x->info->lvl, y->info->lvl));
    level_set(rop->info->lvl, x->info->lvl);
    return 0;
}

static int
_encoding_sub(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp;
    if (!level_eq(x->info->lvl, y->info->lvl)) {
        printf("[%s] unequal levels!\nx=\n", __func__);
        level_print(x->info->lvl);
        printf("y=\n");
        level_print(y->info->lvl);
        assert(level_eq(x->info->lvl, y->info->lvl));
        return 1;
    }
    level_set(rop->info->lvl, x->info->lvl);
    return 0;
}
    
static int
_encoding_is_zero(const pp_vtable *const vt, const encoding *const x,
                  const public_params *const pp)
{
    if (!level_eq(x->info->lvl, vt->toplevel(pp))) {
        puts("this level:");
        level_print(x->info->lvl);
        puts("top level:");
        level_print(vt->toplevel(pp));
        return 1;
    }
    return 0;
}

static void
_encoding_fread(encoding *const x, FILE *const fp)
{
    x->info = calloc(1, sizeof(encoding_info));
    x->info->lvl = calloc(1, sizeof(level));
    level_fread(x->info->lvl, fp);
    GET_NEWLINE(fp);
}

static void
_encoding_fwrite(const encoding *const x, FILE *const fp)
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
    .fwrite = _encoding_fwrite
};

const encoding_vtable *
ab_get_encoding_vtable(const mmap_vtable *const mmap)
{
    ab_encoding_vtable.mmap = mmap;
    return &ab_encoding_vtable;
}
