#include "index_set.h"
#include "util.h"

#include <assert.h>
#include <string.h>

index_set *
index_set_new(size_t nzs)
{
    index_set *ix = my_calloc(1, sizeof ix[0]);
    ix->nzs = nzs;
    ix->pows = my_calloc(ix->nzs, sizeof ix->pows[0]);
    return ix;
}

void
index_set_free(index_set *ix)
{
    if (ix) {
        if (ix->pows)
            free(ix->pows);
        free(ix);
    }
}

void
index_set_clear(index_set *ix)
{
    memset(ix->pows, '\0', ix->nzs * sizeof ix->pows[0]);
}

void
index_set_print(const index_set *ix)
{
    if (ix == NULL)
        return;
    fprintf(stderr, "%lu: ", ix->nzs);
    for (size_t i = 0; i < ix->nzs; ++i) {
        fprintf(stderr, "%d ", ix->pows[i]);
    }
    fprintf(stderr, "\n");
}

void
index_set_add(index_set *rop, const index_set *x, const index_set *y)
{
    for (size_t i = 0; i < rop->nzs; ++i)
        rop->pows[i] = x->pows[i] + y->pows[i];
}

void
index_set_set(index_set *rop, const index_set *x)
{
    for (size_t i = 0; i < x->nzs; i++)
        rop->pows[i] = x->pows[i];
}

index_set *
index_set_copy(const index_set *x)
{
    index_set *rop;
    if ((rop = index_set_new(x->nzs)) == NULL)
        return NULL;
    index_set_set(rop, x);
    return rop;
}

bool
index_set_eq(const index_set *x, const index_set *y)
{
    if (x == NULL && y == NULL)
        return true;
    for (size_t i = 0; i < x->nzs; ++i) {
        if (x->pows[i] != y->pows[i])
            return false;
    }
    return true;
}

index_set *
index_set_union(const index_set *x, const index_set *y)
{
    index_set *rop;
    if ((rop = index_set_new(x->nzs)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        rop->pows[i] = x->pows[i] > y->pows[i] ? x->pows[i] : y->pows[i];
    }
    return rop;
}

index_set *
index_set_difference(const index_set *x, const index_set *y)
{
    index_set *rop;
    if ((rop = index_set_new(x->nzs)) == NULL)
        return NULL;
    for (size_t i = 0; i < x->nzs; i++) {
        if (x->pows[i] < y->pows[i]) {
            fprintf(stderr, "error: negative difference in index set\n");
            index_set_print(x);
            index_set_print(y);
            goto error;
        }
        rop->pows[i] = x->pows[i] - y->pows[i];
    }
    return rop;
error:
    free(rop);
    return NULL;
}

index_set *
index_set_fread(FILE *fp)
{
    index_set *ix;

    if ((ix = my_calloc(1, sizeof ix[0])) == NULL)
        return NULL;
    if (size_t_fread(&ix->nzs, fp) == ERR)
        goto error;
    if ((ix->pows = my_calloc(ix->nzs, sizeof ix->pows[0])) == NULL)
        goto error;
    for (size_t i = 0; i < ix->nzs; i++)
        if (int_fread(&ix->pows[i], fp) == ERR)
            goto error;
    return ix;
error:
    if (ix->pows)
        free(ix->pows);
    free(ix);
    return NULL;
}

int
index_set_fwrite(const index_set *ix, FILE *fp)
{
    if (ix == NULL || fp == NULL)
        return ERR;
    if (size_t_fwrite(ix->nzs, fp) == ERR)
        return ERR;
    for (size_t i = 0; i < ix->nzs; i++) {
        if (int_fwrite(ix->pows[i], fp) == ERR)
            return ERR;
    }
    return OK;
}
