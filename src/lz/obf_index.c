#include "../util.h"

#include <assert.h>
#include <string.h>

#define IX_Y(ix)           (ix)->pows[0]
#define IX_S(ix, op, k, s) (ix)->pows[1 + (op)->q * (k) + (s)]
#define IX_Z(ix, op, k)    (ix)->pows[1 + (op)->q * (op)->c + (k)]
#define IX_W(ix, op, k)    (ix)->pows[1 + (1 + (op)->q) * (op)->c + (k)]

typedef struct {
    size_t *pows;
    size_t nzs;
} obf_index;

static obf_index *
obf_index_new(const obf_params_t *op)
{
    obf_index *ix = my_calloc(1, sizeof(obf_index));
    ix->nzs = (2 + op->q) * op->c + 1;
    ix->pows = my_calloc(ix->nzs, sizeof(size_t));
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
obf_index_clear(obf_index *ix)
{
    memset(ix->pows, '\0', sizeof(unsigned long) * ix->nzs);
}

static void
obf_index_print(const obf_index *ix)
{
    for (size_t i = 0; i < ix->nzs; ++i) {
        fprintf(stderr, "%lu ", ix->pows[i]);
    }
    fprintf(stderr, "\n");
}

static obf_index *
obf_index_new_toplevel(const obf_params_t *op)
{
    obf_index *ix;
    if ((ix = obf_index_new(op)) == NULL)
        return NULL;
    IX_Y(ix) = acirc_max_const_degree(op->circ);
    for (size_t k = 0; k < op->c; k++) {
        /* const sym_id sym = op->chunker(k, op->circ, op->c); */
        for (size_t s = 0; s < op->q; s++) {
            IX_S(ix, op, k, s) = acirc_max_var_degree(op->circ, k);
        }
        IX_Z(ix, op, k) = 1;
        IX_W(ix, op, k) = 1;
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

static obf_index *
obf_index_copy(const obf_index *x, const obf_params_t *op)
{
    obf_index *rop = obf_index_new(op);
    obf_index_set(rop, x);
    return rop;
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
        res->pows[i] = x->pows[i] > y->pows[i] ? x->pows[i] : y->pows[i];
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
    ix->pows = my_calloc(ix->nzs, sizeof(size_t));
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fread(&ix->pows[i], fp);
    }
    return ix;
}

static int
obf_index_fwrite(const obf_index *ix, FILE *fp)
{
    ulong_fwrite(ix->nzs, fp);
    for (size_t i = 0; i < ix->nzs; i++) {
        ulong_fwrite(ix->pows[i], fp);
    }
    return OK;
}
