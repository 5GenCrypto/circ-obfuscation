#pragma once

#include "obf_params.h"
#include "../mmap.h"

#include <stdio.h>

typedef struct {
    size_t q;                   /* # symbols in alphabet */
    size_t c;                   /* # symbols in input */
    size_t gamma;
    size_t **mat;
    size_t *vec;
} level;

level *
level_new(const obf_params_t *op);
void
level_free(level *lvl);
void
level_fprint(FILE *fp, const level *lvl);
void
level_set(level *rop, const level *lvl);
void
level_add(level *rop, const level *x, const level *y);
void
level_mul_ui(level *rop, const level *op, int x);
int
level_flatten(int *pows, const level *lvl);
bool
level_eq(const level *x, const level *y);
// for testing whether we can use constrained addition, we dont consider the
// degree (bottom right corner of the level)
bool
level_eq_z(level *x, level *y);
level *
level_create_vstar(const obf_params_t *op);
level *
level_create_vks(const obf_params_t *op, size_t k, size_t s);
level *
level_create_vc(const obf_params_t *op);
level *
level_create_vhatkso(const obf_params_t *op, size_t k, size_t s, size_t o);
level *
level_create_vhato(const obf_params_t *op, size_t o);
level *
level_create_vbaro(const obf_params_t *op, size_t o);
level *
level_create_vzt(const obf_params_t *op);
void
level_fwrite(const level *lvl, FILE *fp);
void
level_fread(level *lvl, FILE *fp);
