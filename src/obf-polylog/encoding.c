#include "obf_params.h"
#include "../vtables.h"
#include "../util.h"

#include <mmap/mmap_clt_pl.h>
#include <string.h>

struct encoding_info {
    index_set *ix;
    bool polylog;
};
#define my(x) x->info

static int
_encoding_new(const pp_vtable *vt, encoding *enc, const public_params *pp)
{
    (void) pp;
    if ((my(enc) = calloc(1, sizeof my(enc)[0])) == NULL)
        return ERR;
    my(enc)->ix = index_set_new(obf_params_nzs(pp_circ(pp)));
    if (vt->mmap == &clt_pl_vtable)
        my(enc)->polylog = true;
    return OK;
}

static void
_encoding_free(encoding *enc)
{
    if (my(enc)) {
        if (my(enc)->ix)
            index_set_free(my(enc)->ix);
        free(my(enc));
    }
}

static int
_encoding_print(const encoding *enc)
{
    index_set_print(my(enc)->ix);
    return OK;
}

static int *
_encode(encoding *rop, const void *ix_)
{
    int *pows;
    const index_set *const ix = ix_;
    index_set_set(my(rop)->ix, ix);
    if ((pows = calloc(ix->nzs, sizeof pows[0])) == NULL)
        return NULL;
    memcpy(pows, ix->pows, ix->nzs * sizeof pows[0]);
    return pows;
}

static int
_encoding_set(encoding *rop, const encoding *x)
{
    index_set_set(my(rop)->ix, my(x)->ix);
    return OK;
}

static int
_encoding_mul(const pp_vtable *vt, encoding *rop, const encoding *x,
              const encoding *y, const public_params *pp)
{
    (void) vt; (void) pp;
    index_set_add(my(rop)->ix, my(x)->ix, my(y)->ix);
    return OK;
}

static int
_encoding_add(const pp_vtable *vt, encoding *rop, const encoding *x,
              const encoding *y, const public_params *pp)
{
    (void) vt; (void) pp; (void) y;
    index_set_set(my(rop)->ix, my(x)->ix);
    return OK;
}

static int
_encoding_sub(const pp_vtable *vt, encoding *rop, const encoding *x,
              const encoding *y, const public_params *pp)
{
    (void) vt; (void) pp; (void) y;
    index_set_set(my(rop)->ix, my(x)->ix);
    return OK;
}

static int
_encoding_is_zero(const pp_vtable *vt, const encoding *x, const public_params *pp)
{
    const index_set *const toplevel = vt->toplevel(pp);
    if (!index_set_eq(my(x)->ix, toplevel)) {
        fprintf(stderr, "error: index sets not equal\n");
        index_set_print(my(x)->ix);
        index_set_print(toplevel);
        return ERR;
    }
    return OK;
}

static int
_encoding_fread(encoding *x, FILE *fp)
{
    my(x) = calloc(1, sizeof x->info[0]);
    if ((my(x)->ix = index_set_fread(fp)) == NULL)
        goto error;
    return OK;
error:
    free(my(x));
    return ERR;
}

static int
_encoding_fwrite(const encoding *x, FILE *fp)
{
    return index_set_fwrite(my(x)->ix, fp);
}

static const void *
_encoding_mmap_set(const encoding *enc)
{
    return my(enc)->ix;
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
