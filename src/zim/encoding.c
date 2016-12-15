#include "encoding.h"

#include "obf_index.h"
#include "obf_params.h"
#include "public_params.h"
#include "vtables.h"

#include <string.h>

struct encoding_info {
    obf_index *index;
};
#define my(x) x->info

static int
_encoding_new(const pp_vtable *const vt, encoding *const enc,
              const public_params *const pp)
{
    (void) vt;
    enc->info = calloc(1, sizeof(encoding_info));
    enc->info->index = obf_index_create(zim_pp_op(pp)->ninputs);
    return 0;
}

static void
_encoding_free(encoding *const enc)
{
    if (enc->info) {
        if (enc->info->index) {
            obf_index_destroy(enc->info->index);
        }
    }
}

static int
_encoding_print(const encoding *const enc)
{
    obf_index_print(enc->info->index);
    return 0;
}

static int *
_encode(encoding *const rop, const void *const set)
{
    int *pows;
    const obf_index *const ix = (const obf_index *const) set;

    rop->info->index = obf_index_copy(ix);
    pows = my_calloc(ix->n, sizeof(int));
    memcpy(pows, ix->pows, ix->n * sizeof(int));
    return pows;
}

static int
_encoding_set(encoding *const rop, const encoding *const x)
{
    obf_index_set(my(rop)->index, my(x)->index);
    return 0;
}

static int
_encoding_mul(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp;
    obf_index_add(my(rop)->index, my(x)->index, my(y)->index);
    return 0;
}

static int
_encoding_add(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp; (void) y;
    obf_index_set(my(rop)->index, my(x)->index);
    return 0;
}

static int
_encoding_sub(const pp_vtable *const vt, encoding *const rop,
              const encoding *const x, const encoding *const y,
              const public_params *const pp)
{
    (void) vt; (void) pp; (void) y;
    obf_index_set(my(rop)->index, my(x)->index);
    return 0;
}

static int
_encoding_is_zero(const pp_vtable *const vt, const encoding *const x,
                  const public_params *const pp)
{
    const obf_index *const toplevel = vt->toplevel(pp);
    if (!obf_index_eq(my(x)->index, toplevel)) {
        printf("index sets not equal\n");
        obf_index_print(my(x)->index);
        obf_index_print(toplevel);
        return ERR;
    }
    return OK;
}

static void
_encoding_fread(encoding *const x, FILE *const fp)
{
    x->info = calloc(1, sizeof(encoding_info));
    x->info->index = obf_index_fread(fp);
}

static void
_encoding_fwrite(const encoding *const x, FILE *const fp)
{
    obf_index_fwrite(my(x)->index, fp);
}

static void *
_encoding_mmap_set(const encoding *const enc)
{
    return my(enc)->index;
}

static encoding_vtable zim_encoding_vtable =
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

encoding_vtable *
zim_get_encoding_vtable(const mmap_vtable *const mmap)
{
    zim_encoding_vtable.mmap = mmap;
    return &zim_encoding_vtable;
}

const obf_index *
zim_encoding_index(const encoding *const enc)
{
    return enc->info->index;
}
