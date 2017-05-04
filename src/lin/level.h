#pragma once

#include "obf_params.h"
#include "mmap.h"

typedef struct {
    size_t q;                   /* # symbols in alphabet */
    size_t c;                   /* # symbols in input */
    size_t gamma;
    size_t **mat;
    size_t *vec;
} level;

level *
level_new(const circ_params_t *cp);
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
bool
level_eq_z(level *x, level *y);
level *
level_create_vstar(const circ_params_t *cp);
level *
level_create_vks(const circ_params_t *cp, size_t k, size_t s);
level *
level_create_vc(const circ_params_t *cp);
level *
level_create_vhatkso(const circ_params_t *cp, size_t k, size_t s, size_t o, int **types);
level *
level_create_vhato(const circ_params_t *cp, size_t o, size_t M, int **types);
level *
level_create_vbaro(const circ_params_t *cp, size_t o);
level *
level_create_vzt(const circ_params_t *cp, size_t M, size_t D);
void
level_fwrite(const level *lvl, FILE *fp);
void
level_fread(level *lvl, FILE *fp);
