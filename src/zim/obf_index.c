#include "../util.h"

#include <assert.h>

#define IX_Y(IX)       ((IX)->pows[0])
#define IX_X(IX, I, B) ((IX)->pows[(1 + 2*(I) + (B))])
#define IX_Z(IX, I)    ((IX)->pows[(1 + (2*(IX)->n) + (I))])
#define IX_W(IX, I)    ((IX)->pows[(1 + (3*(IX)->n) + (I))])

typedef struct {
    unsigned long *pows;
    size_t nzs;
    size_t n;       // number of inputs to the circuit
} obf_index;

static void obf_index_init(obf_index *ix, size_t n)
{
    ix->n = n;
    ix->nzs = 4 * ix->n + 1;
    ix->pows = my_calloc(ix->nzs, sizeof(unsigned long));
}

static obf_index * obf_index_create(size_t n)
{
    obf_index *ix = my_calloc(1, sizeof(obf_index));
    obf_index_init(ix, n);
    return ix;
}

static void obf_index_destroy(obf_index *ix)
{
    if (ix) {
        if (ix->pows)
            free(ix->pows);
        free(ix);
    }
}

static void obf_index_print(const obf_index *ix)
{
    array_print_ui(ix->pows, ix->nzs);
    printf("\tn=%lu, nzs=%lu\n", ix->n, ix->nzs);
}

static obf_index *
obf_index_create_toplevel(acirc *c)
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

static void obf_index_add(obf_index *rop, const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    assert(y->nzs == rop->nzs);
    array_add(rop->pows, x->pows, y->pows, rop->nzs);
}

static void obf_index_set(obf_index *rop, const obf_index *x)
{
    assert(rop->nzs == x->nzs);
    for (size_t i = 0; i < x->nzs; i++)
        rop->pows[i] = x->pows[i];
}

static obf_index * obf_index_copy(obf_index *ix)
{
    obf_index *new = obf_index_create(ix->n);
    obf_index_set(new, ix);
    return new;
}

static bool obf_index_eq(const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    return array_eq(x->pows, y->pows, x->nzs);
}

static obf_index * obf_index_union(const obf_index *x, const obf_index *y)
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

static obf_index * obf_index_difference(const obf_index *x, const obf_index *y)
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

static obf_index * obf_index_fread(FILE *fp)
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

static int obf_index_fwrite(const obf_index *ix, FILE *fp)
{
    ulong_fwrite(ix->nzs, fp);
    ulong_fwrite(ix->n, fp);
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fwrite(ix->pows[i], fp);
    }
    return OK;
}
