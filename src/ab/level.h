#ifndef __AB__LEVEL_H__
#define __AB__LEVEL_H__

#include "mmap.h"
#include "input_chunker.h"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    size_t nrows;
    size_t ncols;
    size_t **mat;
} level;

void level_print   (level *lvl);
void level_set     (level *rop, const level *lvl);
void level_add     (level *rop, const level *x, const level *y);
void level_mul_ui  (level *rop, level *op, int x);
void level_flatten (int *pows, const level *lvl);
int  level_eq      (level *x, level *y);
int  level_eq_z    (level *x, level *y);

level *
level_new(const obf_params_t *const p);
void
level_free(level *lvl);

level *
level_create_v_ib(const obf_params_t *const op, size_t i, bool b);
level *
level_create_v_i(const obf_params_t *const op, size_t i);
level *
level_create_v_hat_ib_o(const obf_params_t *const op, size_t i, bool b, size_t o);
level *
level_create_v_0(const obf_params_t *const op);
level *
level_create_v_hat_o(const obf_params_t *const op, size_t o);
level *
level_create_v_star(const obf_params_t *const op);
level *
level_create_vzt(const obf_params_t *const op);

void
level_fwrite(const level *const lvl, FILE *const fp);
void
level_fread(level *lvl, FILE *const fp);

#endif
