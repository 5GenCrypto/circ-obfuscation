#include "obf_index.h"
#include "../util.h"

#include <assert.h>

void obf_index_init(obf_index *ix, size_t n)
{
    ix->n = n;
    ix->nzs = 4 * ix->n + 1;
    ix->pows = my_calloc(ix->nzs, sizeof(size_t));
}

obf_index * obf_index_new(size_t n)
{
    obf_index *ix = my_calloc(1, sizeof(obf_index));
    obf_index_init(ix, n);
    return ix;
}

void obf_index_free(obf_index *ix)
{
    if (ix) {
        if (ix->pows)
            free(ix->pows);
        free(ix);
    }
}

void obf_index_print(const obf_index *ix)
{
    array_print_ui(ix->pows, ix->nzs);
    printf("\tn=%lu, nzs=%lu\n", ix->n, ix->nzs);
}

obf_index *
obf_index_new_toplevel(acirc *c)
{
    obf_index *ix;
    if ((ix = obf_index_new(c->ninputs)) == NULL)
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

void obf_index_add(obf_index *rop, const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    assert(y->nzs == rop->nzs);
    array_add(rop->pows, x->pows, y->pows, rop->nzs);
}

void obf_index_set(obf_index *rop, const obf_index *x)
{
    assert(rop->nzs == x->nzs);
    for (size_t i = 0; i < x->nzs; i++)
        rop->pows[i] = x->pows[i];
}

obf_index * obf_index_copy(obf_index *ix)
{
    obf_index *new = obf_index_new(ix->n);
    obf_index_set(new, ix);
    return new;
}

bool obf_index_eq(const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    return array_eq(x->pows, y->pows, x->nzs);
}

obf_index * obf_index_union(const obf_index *x, const obf_index *y)
{
    obf_index *res;
    assert(x->nzs == y->nzs);
    if ((res = obf_index_new(x->n)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = MAX(x->pows[i], y->pows[i]);
    }
    return res;
}

obf_index * obf_index_difference(const obf_index *x, const obf_index *y)
{
    obf_index *res;
    assert(x->nzs == y->nzs);
    if ((res = obf_index_new(x->n)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = x->pows[i] - y->pows[i];
    }
    return res;
}

obf_index * obf_index_fread(FILE *fp)
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

int obf_index_fwrite(const obf_index *ix, FILE *fp)
{
    ulong_fwrite(ix->nzs, fp);
    ulong_fwrite(ix->n, fp);
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fwrite(ix->pows[i], fp);
    }
    return OK;
}
