#pragma once

#include "circ_params.h"

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    int *pows;
    size_t nzs;
} index_set;

index_set *
index_set_new(size_t nzs);
void
index_set_free(index_set *ix);
void
index_set_clear(index_set *ix);
void
index_set_print(const index_set *ix);
void
index_set_add(index_set *rop, const index_set *x, const index_set *y);
void
index_set_set(index_set *rop, const index_set *x);
index_set *
index_set_copy(const index_set *x);
bool
index_set_eq(const index_set *x, const index_set *y);
index_set *
index_set_union(const index_set *x, const index_set *y);
index_set *
index_set_difference(const index_set *x, const index_set *y);
index_set *
index_set_fread(FILE *fp);
int
index_set_fwrite(const index_set *ix, FILE *fp);
