#include "../util.h"

#include <assert.h>

#define IX_Y(IX)       ((IX)->pows[0])
#define IX_X(IX, I, B) ((IX)->pows[(1 + 2*(I) + (B))])
#define IX_Z(IX, I)    ((IX)->pows[(1 + (2*(IX)->n) + (I))])
#define IX_W(IX, I)    ((IX)->pows[(1 + (3*(IX)->n) + (I))])

typedef struct {
    unsigned long *pows;
    size_t n;
    size_t nzs;
} obf_index;

static obf_index *
obf_index_new(const obf_params_t *op)
{
    obf_index *ix = my_calloc(1, sizeof(obf_index));
    ix->n = op->c;
    ix->nzs = (2 + op->ell) * op->c + 1;
    ix->pows = my_calloc(ix->nzs, sizeof(unsigned long));
    return ix;
}

static void
obf_index_free(obf_index *ix)
{
    if (ix) {
        if (ix->pows)
            free(ix->pows);
        free(ix);
    }
}

static void
obf_index_print(const obf_index *ix)
{
    (void) ix;
}

static obf_index *
obf_index_new_toplevel(const obf_params_t *op)
{
    obf_index *ix;
    if ((ix = obf_index_new(op)) == NULL)
        return NULL;
    IX_Y(ix) = acirc_max_const_degree(op->circ);
    for (size_t i = 0; i < op->c; i++) {
        const size_t d = acirc_max_var_degree(op->circ, i);
        for (size_t j = 0; j < op->ell; ++j) {
            IX_X(ix, i, j) = d;            
        }
        IX_Z(ix, i) = 1;
        IX_W(ix, i) = 1;
    }
    return ix;
}

static void
obf_index_add(obf_index *rop, const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    assert(y->nzs == rop->nzs);
    array_add(rop->pows, x->pows, y->pows, rop->nzs);
}

static void
obf_index_set(obf_index *rop, const obf_index *x)
{
    assert(rop->nzs == x->nzs);
    for (size_t i = 0; i < x->nzs; i++)
        rop->pows[i] = x->pows[i];
}

static bool
obf_index_eq(const obf_index *x, const obf_index *y)
{
    assert(x->nzs == y->nzs);
    return array_eq(x->pows, y->pows, x->nzs);
}

static obf_index *
obf_index_union(const obf_params_t *op, const obf_index *x, const obf_index *y)
{
    obf_index *res;
    assert(x->nzs == y->nzs);
    if ((res = obf_index_new(op)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = max(x->pows[i], y->pows[i]);
    }
    return res;
}

static obf_index *
obf_index_difference(const obf_params_t *op, const obf_index *x, const obf_index *y)
{
    obf_index *res;
    assert(x->nzs == y->nzs);
    if ((res = obf_index_new(op)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        res->pows[i] = x->pows[i] - y->pows[i];
    }
    return res;
}

static obf_index *
obf_index_fread(FILE *fp)
{
    obf_index *ix = my_calloc(1, sizeof(obf_index));
    ulong_fread(&ix->nzs, fp);
    ulong_fread(&ix->n, fp);
    ix->pows = my_calloc(ix->nzs, sizeof(unsigned long));
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fread(&ix->pows[i], fp);
    }
    return ix;
}

static int
obf_index_fwrite(const obf_index *ix, FILE *fp)
{
    ulong_fwrite(ix->nzs, fp);
    ulong_fwrite(ix->n, fp);
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fwrite(ix->pows[i], fp);
    }
    return OK;
}
