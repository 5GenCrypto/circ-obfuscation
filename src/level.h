#ifndef __SRC_LEVEL_H__
#define __SRC_LEVEL_H__

#include "circuit.h"
#include "input_chunker.h"
#include "params.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t **mat;
    uint32_t *vec;
    params *p;
} level;

void level_init  (level *lvl, params *p);
void level_clear (level *lvl);
void level_print (level *lvl);
void level_set   (level *rop, level *lvl);

void level_set_vstar   (level *lvl);
void level_set_vsk     (level *lvl, size_t s, size_t k);
void level_set_vc      (level *lvl);
void level_set_vhatsok (level *lvl, size_t s, size_t o, size_t k);
void level_set_vhato   (level *lvl, size_t o);
void level_set_vbaro   (level *lvl, size_t o);
void level_set_vzt     (level *lvl);

#endif
