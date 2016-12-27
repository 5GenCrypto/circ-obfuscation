#ifndef __ZIM__OBF_INDEX__
#define __ZIM__OBF_INDEX__

#include "util.h"
#include <acirc.h>
#include <stdbool.h>
#include <stdio.h>

#define IX_Y(IX)       ((IX)->pows[0])
#define IX_X(IX, I, B) ((IX)->pows[(1 + 2*(I) + (B))])
#define IX_Z(IX, I)    ((IX)->pows[(1 + (2*(IX)->n) + (I))])
#define IX_W(IX, I)    ((IX)->pows[(1 + (3*(IX)->n) + (I))])

typedef struct {
    unsigned long *pows;
    size_t nzs;
    size_t n;       // number of inputs to the circuit
} obf_index;

obf_index * obf_index_create(size_t n);
void obf_index_destroy(obf_index *ix);

void obf_index_add(obf_index *const rop, const obf_index *const x,
                   const obf_index *const y);
void obf_index_set(obf_index *const rop, const obf_index *const x);
bool obf_index_eq(const obf_index *const x, const obf_index *const y);

obf_index * obf_index_union(const obf_index *const x, const obf_index *const y);
obf_index * obf_index_difference(const obf_index *const x, const obf_index *const y);

void obf_index_print(const obf_index *const ix);
obf_index * obf_index_fread(FILE *const fp);
int obf_index_fwrite(const obf_index *const ix, FILE *const fp);

obf_index *
obf_index_create_toplevel(acirc *const c);

#endif
