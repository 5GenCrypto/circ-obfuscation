#pragma once

#include "obf_params.h"

#include <stdlib.h>

typedef struct {
    int *pows;
    size_t nzs;
} obf_index;

#define IX_Y(ix)           (ix)->pows[0]
#define IX_S(ix, op, k, s) (ix)->pows[1 + (op)->q * (k) + (s)]
#define IX_Z(ix, op, k)    (ix)->pows[1 + (op)->q * (op)->c + (k)]
#define IX_W(ix, op, k)    (ix)->pows[1 + (1 + (op)->q) * (op)->c + (k)]

obf_index *
obf_index_new(const obf_params_t *op);
void
obf_index_free(obf_index *ix);
void
obf_index_clear(obf_index *ix);
void
obf_index_print(const obf_index *ix);
obf_index *
obf_index_new_toplevel(const obf_params_t *op);
void
obf_index_add(obf_index *rop, const obf_index *x, const obf_index *y);
void
obf_index_set(obf_index *rop, const obf_index *x);
obf_index *
obf_index_copy(const obf_index *x, const obf_params_t *op);
bool
obf_index_eq(const obf_index *x, const obf_index *y);
obf_index *
obf_index_union(const obf_params_t *op, const obf_index *x, const obf_index *y);
obf_index *
obf_index_difference(const obf_params_t *op, const obf_index *x, const obf_index *y);
obf_index *
obf_index_fread(FILE *fp);
int
obf_index_fwrite(const obf_index *ix, FILE *fp);
