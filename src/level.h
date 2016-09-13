#ifndef __SRC_LEVEL_H__
#define __SRC_LEVEL_H__

#include "input_chunker.h"
#include "obf_params.h"
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
level_new(obf_params *p);
void
level_free(level *lvl);

level *
level_create_v_ib(obf_params *p, size_t i, bool b);
level *
level_create_v_i(obf_params *p, size_t i);
level *
level_create_v_hat_ib_o(obf_params *p, size_t i, bool b, size_t o);
level *
level_create_v_0(obf_params *p);
level *
level_create_v_hat_o(obf_params *op, size_t o);
level *
level_create_v_star(obf_params *p);
level *
level_create_vzt(obf_params *p);

void level_write (FILE *const fp, const level *lvl);
void level_read  (level *lvl, FILE *const fp);

#endif
