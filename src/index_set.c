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
    for (size_t i = 0; i < ix->nzs; ++i) {
        fprintf(stderr, "%d ", ix->pows[i]);
    }
    fprintf(stderr, "\n");
}

void
index_set_add(index_set *rop, const index_set *x, const index_set *y)
{
    array_add(rop->pows, x->pows, y->pows, rop->nzs);
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
    return array_eq(x->pows, y->pows, x->nzs);
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
        rop->pows[i] = x->pows[i] - y->pows[i];
    }
    return rop;
}

index_set *
index_set_fread(FILE *fp)
{
    index_set *ix = my_calloc(1, sizeof ix[0]);
    ulong_fread(&ix->nzs, fp);
    ix->pows = my_calloc(ix->nzs, sizeof ix->pows[0]);
    for (size_t i = 0; i < ix->nzs; i++) {
        int_fread(&ix->pows[i], fp);
    }
    return ix;
}

int
index_set_fwrite(const index_set *ix, FILE *fp)
{
    ulong_fwrite(ix->nzs, fp);
    for (size_t i = 0; i < ix->nzs; i++) {
        int_fwrite(ix->pows[i], fp);
    }
    return OK;
}
