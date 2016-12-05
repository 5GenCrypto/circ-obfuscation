#include "vtables.h"
#include "level.h"
#include "util.h"

#include <assert.h>

struct encoding_info {
    level *lvl;
};

static int
ab_encoding_new(const pp_vtable *const vt, encoding *const enc,
                const public_params *const pp)
{
    enc->info = calloc(1, sizeof(encoding_info));
    enc->info->lvl = level_new((obf_params_t *) vt->params(pp));
    return 0;
}

static void
ab_encoding_free(encoding *const enc)
{
    if (enc->info) {
        if (enc->info->lvl) {
            level_free(enc->info->lvl);
        }
        free(enc->info);
    }
}

static int
ab_encoding_print(const encoding *const enc)
{
    level_print(enc->info->lvl);
    return 0;
}

static int
ab_encode(int *const pows, encoding *const rop, const void *const set)
{
    const level *const lvl = (const level *const) set;
    level_set(rop->info->lvl, lvl);
    level_flatten(pows, lvl);
    return 0;
}

static int
ab_encoding_set(encoding *const rop, const encoding *const x)
{
    level_set(rop->info->lvl, x->info->lvl);
    return 0;
}

static int
ab_encoding_mul(const pp_vtable *const vt, encoding *const rop,
                const encoding *const x, const encoding *const y,
                const public_params *const pp)
{
    (void) vt; (void) pp;
    level_add(rop->info->lvl, x->info->lvl, y->info->lvl);
    return 0;
}

static int
ab_encoding_add(const pp_vtable *const vt, encoding *const rop,
                const encoding *const x, const encoding *const y,
                const public_params *const pp)
{
    (void) vt; (void) pp;
    assert(level_eq(x->info->lvl, y->info->lvl));
    level_set(rop->info->lvl, x->info->lvl);
    return 0;
}

static int
ab_encoding_sub(const pp_vtable *const vt, encoding *const rop,
                const encoding *const x, const encoding *const y,
                const public_params *const pp)
{
    (void) vt; (void) pp;
    if (!level_eq(x->info->lvl, y->info->lvl)) {
        printf("[encoding_sub] unequal levels!\nx=\n");
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
ab_encoding_is_zero(const pp_vtable *const vt, const encoding *const x,
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
ab_encoding_fread(encoding *const x, FILE *const fp)
{
    x->info = calloc(1, sizeof(encoding_info));
    x->info->lvl = calloc(1, sizeof(level));
    level_fread(x->info->lvl, fp);
    GET_NEWLINE(fp);
}

static void
ab_encoding_fwrite(const encoding *const x, FILE *const fp)
{
    level_fwrite(x->info->lvl, fp);
    PUT_NEWLINE(fp);
}

static encoding_vtable ab_encoding_vtable =
{
    .mmap = NULL,
    .new = ab_encoding_new,
    .free = ab_encoding_free,
    .print = ab_encoding_print,
    .encode = ab_encode,
    .set = ab_encoding_set,
    .mul = ab_encoding_mul,
    .add = ab_encoding_add,
    .sub = ab_encoding_sub,
    .is_zero = ab_encoding_is_zero,
    .fread = ab_encoding_fread,
    .fwrite = ab_encoding_fwrite
};

const encoding_vtable *
ab_get_encoding_vtable(const mmap_vtable *const mmap)
{
    ab_encoding_vtable.mmap = mmap;
    return &ab_encoding_vtable;
}
