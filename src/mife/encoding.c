#include "mife_params.h"
#include "../vtables.h"
#include "../util.h"

#include <string.h>

struct encoding_info {
    index_set *index;
};
#define my(x) x->info

static int
_encoding_new(const pp_vtable *vt, encoding *enc, const public_params *pp)
{
    const obf_params_t *const mp = vt->params(pp);
    if ((enc->info = calloc(1, sizeof enc->info[0])) == NULL)
        return ERR;
    enc->info->index = index_set_new(mife_params_nzs(&mp->cp));
    return OK;
}

static void
_encoding_free(encoding *enc)
{
    if (enc->info) {
        if (enc->info->index)
            index_set_free(enc->info->index);
        free(enc->info);
    }
}

static int
_encoding_print(const encoding *enc)
{
    index_set_print(enc->info->index);
    return OK;
}

static int *
_encode(encoding *rop, const void *set)
{
    int *pows;
    const index_set *const ix = set;

    index_set_set(rop->info->index, ix);
    pows = calloc(ix->nzs, sizeof pows[0]);
    if (pows)
        memcpy(pows, ix->pows, ix->nzs * sizeof pows[0]);
    return pows;
}

static int
_encoding_set(encoding *rop, const encoding *x)
{
    index_set_set(my(rop)->index, my(x)->index);
    return OK;
}

static int
_encoding_mul(const pp_vtable *vt, encoding *rop, const encoding *x,
              const encoding *y, const public_params *pp)
{
    (void) vt; (void) pp;
    index_set_add(my(rop)->index, my(x)->index, my(y)->index);
    return OK;
}

static int
_encoding_add(const pp_vtable *vt, encoding *rop, const encoding *x,
              const encoding *y, const public_params *pp)
{
    (void) vt; (void) pp; (void) y;
    index_set_set(my(rop)->index, my(x)->index);
    return OK;
}

static int
_encoding_sub(const pp_vtable *vt, encoding *rop, const encoding *x,
              const encoding *y, const public_params *pp)
{
    (void) vt; (void) pp; (void) y;
    index_set_set(my(rop)->index, my(x)->index);
    return OK;
}

static int
_encoding_is_zero(const pp_vtable *vt, const encoding *x, const public_params *pp)
{
    const index_set *const toplevel = vt->toplevel(pp);
    if (!index_set_eq(my(x)->index, toplevel)) {
        fprintf(stderr, "error: index sets not equal\n");
        index_set_print(my(x)->index);
        index_set_print(toplevel);
        return ERR;
    }
    return OK;
}

static int
_encoding_fread(encoding *x, FILE *fp)
{
    x->info = calloc(1, sizeof x->info[0]);
    if ((x->info->index = index_set_fread(fp)) == NULL)
        goto error;
    return OK;
error:
    free(x->info);
    return ERR;
}

static int
_encoding_fwrite(const encoding *x, FILE *fp)
{
    return index_set_fwrite(my(x)->index, fp);
}

static const void *
_encoding_mmap_set(const encoding *enc)
{
    return my(enc)->index;
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
    .mmap_set = _encoding_mmap_set,
};

PRIVATE const encoding_vtable *
get_encoding_vtable(const mmap_vtable *mmap)
{
    _encoding_vtable.mmap = mmap;
    return &_encoding_vtable;
}
