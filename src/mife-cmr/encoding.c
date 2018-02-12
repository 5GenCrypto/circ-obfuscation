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
    (void) vt;
    my(enc) = xcalloc(1, sizeof my(enc)[0]);
    if ((my(enc)->index = index_set_new(mife_params_nzs(pp_circ(pp)))) == NULL)
        goto error;
    return OK;
error:
    if (my(enc))
        free(my(enc));
    return ERR;
}

static void
_encoding_free(encoding *enc)
{
    if (my(enc)) {
        if (my(enc)->index)
            index_set_free(my(enc)->index);
        free(my(enc));
    }
}

static int
_encoding_print(const encoding *enc)
{
    index_set_print(my(enc)->index);
    return OK;
}

static int *
_encode(encoding *rop, const void *ix_)
{
    const index_set *ix = ix_;
    int *pows;
    index_set_set(rop->info->index, ix);
    pows = xcalloc(ix->nzs, sizeof pows[0]);
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
        fprintf(stderr, "%s: %s: index sets not equal\n", errorstr, __func__);
        index_set_print(my(x)->index);
        index_set_print(toplevel);
        return ERR;
    }
    return OK;
}

static int
_encoding_fread(encoding *x, FILE *fp)
{
    my(x) = xcalloc(1, sizeof x->info[0]);
    if ((x->info->index = index_set_fread(fp)) == NULL)
        goto error;
    return OK;
error:
    if (my(x))
        free(my(x));
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
