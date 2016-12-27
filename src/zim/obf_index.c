#include "obf_index.h"

#include <assert.h>

extern bool g_verbose;

static void obf_index_init(obf_index *ix, size_t n)
{
    ix->n = n;
    ix->nzs = 4 * ix->n + 1;
    ix->pows = my_calloc(ix->nzs, sizeof(unsigned long));
}

obf_index * obf_index_create(size_t n)
{
    obf_index *ix = my_calloc(1, sizeof(obf_index));
    obf_index_init(ix, n);
    return ix;
}

void obf_index_destroy(obf_index *ix)
{
    if (ix) {
        if (ix->pows)
            free(ix->pows);
        free(ix);
    }
}

obf_index *
obf_index_create_toplevel(acirc *const c)
{
    obf_index *ix;
    if ((ix = obf_index_create(c->ninputs)) == NULL)
        return NULL;
    IX_Y(ix) = acirc_max_const_degree(c);
    for (size_t i = 0; i < ix->n; i++) {
        const size_t d = acirc_max_var_degree(c, i);
        IX_X(ix, i, 0) = d;
        IX_X(ix, i, 1) = d;
        IX_Z(ix, i) = 1;
        IX_W(ix, i) = 1;
    }
    if (g_debug >= DEBUG) {
        fprintf(stderr, "[%s] toplevel: ", __func__);
        obf_index_print(ix);
    }
    return ix;
}

void obf_index_add(obf_index *const rop, const obf_index *const x,
                   const obf_index *const y)
{
    assert(x->nzs == y->nzs);
    assert(y->nzs == rop->nzs);
    ARRAY_ADD(rop->pows, x->pows, y->pows, rop->nzs);
}

void obf_index_set(obf_index *const rop, const obf_index *const x)
{
    assert(rop->nzs == x->nzs);
    for (size_t i = 0; i < x->nzs; i++)
        rop->pows[i] = x->pows[i];
}

bool obf_index_eq(const obf_index *const x, const obf_index *const y)
{
    assert(x->nzs == y->nzs);
    return ARRAY_EQ(x->pows, y->pows, x->nzs);
}

obf_index * obf_index_union(const obf_index *const x, const obf_index *const y)
{
    obf_index *res;
    assert(x->nzs == y->nzs);
    if ((res = obf_index_create(x->n)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = MAX(x->pows[i], y->pows[i]);
    }
    return res;
}

obf_index * obf_index_difference(const obf_index *const x, const obf_index *const y)
{
    obf_index *res;
    assert(x->nzs == y->nzs);
    if ((res = obf_index_create(x->n)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = x->pows[i] - y->pows[i];
    }
    return res;
}

void obf_index_print(const obf_index *const ix)
{
    array_print_ui(ix->pows, ix->nzs);
    printf("\tn=%lu, nzs=%lu\n", ix->n, ix->nzs);
}

obf_index * obf_index_fread(FILE *const fp)
{
    obf_index *ix = my_calloc(1, sizeof(obf_index));
    ulong_fread(&ix->nzs, fp);
    ulong_fread(&ix->n, fp);
    ix->pows = my_malloc(ix->nzs * sizeof(unsigned long));
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fread(&ix->pows[i], fp);
    }
    return ix;
}

int obf_index_fwrite(const obf_index *const ix, FILE *const fp)
{
    ulong_fwrite(ix->nzs, fp);
    ulong_fwrite(ix->n, fp);
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fwrite(ix->pows[i], fp);
    }
    return 0;
}
