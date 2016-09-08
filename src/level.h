#ifndef __SRC_LEVEL_H__
#define __SRC_LEVEL_H__

#include "input_chunker.h"
#include "obf_params.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    size_t q;                   /* # symbols in alphabet */
    size_t c;                   /* # symbols in input */
    size_t gamma;
    size_t **mat;
    size_t *vec;
} level;

void level_print   (level *lvl);
void level_set     (level *rop, const level *lvl);
void level_add     (level *rop, const level *x, const level *y);
void level_mul_ui  (level *rop, level *op, int x);
void level_flatten (int *pows, const level *lvl);
int  level_eq      (level *x, level *y);
int  level_eq_z    (level *x, level *y);

level * level_new(obf_params *p);
level* level_create_vstar   (obf_params *p);
level* level_create_vks     (obf_params *p, size_t k, size_t s);
level* level_create_vc      (obf_params *p);
level* level_create_vhatkso (obf_params *p, size_t k, size_t s, size_t o);
level* level_create_vhato   (obf_params *p, size_t o);
level* level_create_vbaro   (obf_params *p, size_t o);
level* level_create_vzt     (obf_params *p);
void level_free(level *lvl);

void level_write (FILE *const fp, const level *lvl);
void level_read  (level *lvl, FILE *const fp);

#endif
