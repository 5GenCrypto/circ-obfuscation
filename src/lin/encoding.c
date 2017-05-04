#include "encoding.h"
#include "level.h"
#include "vtables.h"
#include "../util.h"

#include <assert.h>

struct encoding_info {
    level *lvl;
    size_t nslots;
};
#define info(x) (x)->info

bool
encoding_equal(const encoding *x, const encoding *y)
{
    return level_eq(info(x)->lvl, info(y)->lvl);
}

bool
encoding_equal_z(const encoding *x, const encoding *y)
{
    return level_eq_z(info(x)->lvl, info(y)->lvl);
}

static int
_encoding_new(const pp_vtable *vt, encoding *enc, const public_params *pp)
{
    const obf_params_t *op = vt->params(pp);
    info(enc) = my_calloc(1, sizeof info(enc)[0]);
    info(enc)->lvl = level_new(&op->cp);
    info(enc)->nslots = op->cp.n + 3;
    return OK;
}

static void
_encoding_free(encoding *enc)
{
    if (info(enc)) {
        if (info(enc)->lvl)
            level_free(info(enc)->lvl);
        free(info(enc));
    }
}

static int
_encoding_print(const encoding *enc)
{
    fprintf(stderr, "Encoding: ");
    level_fprint(stderr, info(enc)->lvl);
    return OK;
}

static int *
_encode(encoding *rop, const void *set)
{
    int *pows;
    const level *lvl = (const level *const) set;
    level_set(info(rop)->lvl, lvl);
    pows = my_calloc((lvl->q+1) * (lvl->c+2) + lvl->gamma, sizeof(int));
    level_flatten(pows, lvl);
    return pows;
}

static int
_encoding_set(encoding *rop, const encoding *x)
{
    info(rop)->nslots = info(x)->nslots;
    level_set(info(rop)->lvl, info(x)->lvl);
    return OK;
}

static int
_encoding_mul(const pp_vtable *vt, encoding *rop,
              const encoding *x, const encoding *y,
              const public_params *pp)
{
    (void) vt; (void) pp;
    level_add(info(rop)->lvl, info(x)->lvl, info(y)->lvl);
    return OK;
}

static int
_encoding_add(const pp_vtable *vt, encoding *rop,
              const encoding *x, const encoding *y,
              const public_params *pp)
{
    (void) vt; (void) pp;
    if (!level_eq(info(x)->lvl, info(y)->lvl)) {
        fprintf(stderr, "[%s] unequal levels\n", __func__);
        fprintf(stderr, "x: ");
        level_fprint(stderr, x->info->lvl);
        fprintf(stderr, "y: ");
        level_fprint(stderr, y->info->lvl);
        return ERR;
    }
    level_set(info(rop)->lvl, info(x)->lvl);
    return OK;
}

static int
_encoding_sub(const pp_vtable *vt, encoding *rop,
              const encoding *x, const encoding *y,
              const public_params *pp)
{
    (void) vt; (void) pp;
    if (!level_eq(info(x)->lvl, info(y)->lvl)) {
        fprintf(stderr, "[%s] unequal levels\n", __func__);
        fprintf(stderr, "x: ");
        level_fprint(stderr, info(x)->lvl);
        fprintf(stderr, "y: ");
        level_fprint(stderr, info(x)->lvl);
        return ERR;
    }
    level_set(info(rop)->lvl, info(x)->lvl);
    return OK;
}

static int
_encoding_is_zero(const pp_vtable *vt, const encoding *x,
                  const public_params *pp)
{
    if (!level_eq(x->info->lvl, vt->toplevel(pp))) {
        fprintf(stderr, "[%s] unequal levels\n", __func__);
        fprintf(stderr, "this level: ");
        level_fprint(stderr, x->info->lvl);
        fprintf(stderr, "top level:  ");
        level_fprint(stderr, vt->toplevel(pp));
        return ERR;
    }
    return OK;
}

static int
_encoding_fread(encoding *x, FILE *fp)
{
    info(x) = calloc(1, sizeof info(x)[0]);
    info(x)->lvl = calloc(1, sizeof info(x)->lvl[0]);
    level_fread(info(x)->lvl, fp);
    return OK;
}

static int
_encoding_fwrite(const encoding *x, FILE *fp)
{
    level_fwrite(info(x)->lvl, fp);
    return OK;
}

static const void *
_encoding_mmap_set(const encoding *x)
{
    return info(x)->lvl;
}

static encoding_vtable _encoding_vtable =
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

PRIVATE const encoding_vtable *
get_encoding_vtable(const mmap_vtable *mmap)
{
    _encoding_vtable.mmap = mmap;
    return &_encoding_vtable;
}
