#include "vtables.h"
#include "obf_params.h"
#include "../util.h"

#include <assert.h>

struct encoding_info {
    level *lvl;
    size_t nslots;
};
#define info(x) (x)->info

static int
_encoding_new(const pp_vtable *const vt, encoding *const enc,
              const public_params *const pp)
{
    const obf_params_t *const op = vt->params(pp);
    info(enc) = my_calloc(1, sizeof(encoding_info));
    info(enc)->lvl = level_new(op);
    info(enc)->nslots = op->c+3;
    return OK;
}

static void
_encoding_free(encoding *const enc)
{
    if (info(enc)) {
        if (info(enc)->lvl)
            level_free(info(enc)->lvl);
        free(info(enc));
    }
}

static int
_encoding_print(const encoding *const enc)
{
    printf("Encoding:\n");
    level_print(info(enc)->lvl);
    return OK;
}

static int *
_encode(encoding *const rop, const void *const set)
{
    int *pows;
    const level *const lvl = (const level *const) set;
    level_set(info(rop)->lvl, lvl);
    pows = my_calloc((lvl->q+1) * (lvl->c+2) + lvl->gamma, sizeof(int));
    level_flatten(pows, lvl);
    return pows;
}

static int
_encoding_set(encoding *const rop, const encoding *const x)
{
    info(rop)->nslots = info(x)->nslots;
    level_set(info(rop)->lvl, info(x)->lvl);
    return OK;
}

static int
_encoding_mul(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp;
    level_add(info(rop)->lvl, info(x)->lvl, info(y)->lvl);
    return OK;
}

static int
_encoding_add(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp;
    assert(level_eq(info(x)->lvl, info(y)->lvl));
    level_set(info(rop)->lvl, info(x)->lvl);
    return OK;
}

static int
_encoding_sub(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp;
    if (!level_eq(info(x)->lvl, info(y)->lvl)) {
        printf("[%s] unequal levels!\nx=\n", __func__);
        level_print(info(x)->lvl);
        printf("y=\n");
        level_print(info(x)->lvl);
        assert(level_eq(info(x)->lvl, info(y)->lvl));
        return ERR;
    }
    level_set(info(rop)->lvl, info(x)->lvl);
    return OK;
}

static int
_encoding_is_zero(const pp_vtable *const vt, const encoding *const x,
                  const public_params *const pp)
{
    if (!level_eq(info(x)->lvl, vt->toplevel(pp))) {
        puts("this level:");
        level_print(info(x)->lvl);
        puts("top level:");
        level_print(vt->toplevel(pp));
        return ERR;
    }
    return OK;
}

static void
_encoding_fread(encoding *const x, FILE *const fp)
{
    info(x) = calloc(1, sizeof(encoding_info));
    info(x)->lvl = calloc(1, sizeof(level));
    level_fread(info(x)->lvl, fp);
}

static void
_encoding_fwrite(const encoding *const x, FILE *const fp)
{
    level_fwrite(info(x)->lvl, fp);
}

static void *
_encoding_mmap_set(const encoding *const x)
{
    return info(x)->lvl;
}

static encoding_vtable lin_encoding_vtable =
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
    .mmap_set = _encoding_mmap_set
};

const encoding_vtable *
lin_get_encoding_vtable(const mmap_vtable *const mmap)
{
    lin_encoding_vtable.mmap = mmap;
    return &lin_encoding_vtable;
}
