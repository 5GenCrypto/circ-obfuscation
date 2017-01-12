#pragma once

#include <acirc.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned long *pows;
    size_t nzs;
    size_t n;       // number of inputs to the circuit
} obf_index;

#define IX_Y(IX)       ((IX)->pows[0])
#define IX_X(IX, I, B) ((IX)->pows[(1 + 2*(I) + (B))])
#define IX_Z(IX, I)    ((IX)->pows[(1 + (2*(IX)->n) + (I))])
#define IX_W(IX, I)    ((IX)->pows[(1 + (3*(IX)->n) + (I))])

void obf_index_init(obf_index *ix, size_t n);
obf_index * obf_index_new(size_t n);
void obf_index_free(obf_index *ix);
void obf_index_print(const obf_index *ix);
obf_index * obf_index_new_toplevel(acirc *c);

void obf_index_add(obf_index *rop, const obf_index *x, const obf_index *y);
void obf_index_set(obf_index *rop, const obf_index *x);
obf_index * obf_index_copy(obf_index *ix);
bool obf_index_eq(const obf_index *x, const obf_index *y);
obf_index * obf_index_union(const obf_index *x, const obf_index *y);
obf_index * obf_index_difference(const obf_index *x, const obf_index *y);
obf_index * obf_index_fread(FILE *fp);
int obf_index_fwrite(const obf_index *ix, FILE *fp);
