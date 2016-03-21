#ifndef __SRC_LEVEL_H__
#define __SRC_LEVEL_H__

#include "circuit.h"
#include "input_chunker.h"
#include "obf_params.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t **mat;
    uint32_t *vec;
    obf_params *p;
} level;

void level_init    (level *lvl, obf_params *p);
void level_clear   (level *lvl);
void level_destroy (level *lvl);
void level_print   (level *lvl);
void level_set     (level *rop, const level *lvl);
void level_add     (level *rop, const level *x, const level *y);
void level_mul_ui  (level *rop, level *op, int x);

level* level_create_vstar   (obf_params *p);
level* level_create_vks     (obf_params *p, size_t k, size_t s);
level* level_create_vc      (obf_params *p);
level* level_create_vhatkso (obf_params *p, size_t k, size_t s, size_t o);
level* level_create_vhato   (obf_params *p, size_t o);
level* level_create_vbaro   (obf_params *p, size_t o);

#endif
