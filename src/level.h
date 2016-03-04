#ifndef __SRC_LEVEL_H__
#define __SRC_LEVEL_H__

#include "circuit.h"

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t q;
    size_t c;
    size_t gamma;
    uint32_t **mat;
    uint32_t *vec;
} level;

level* level_create(size_t q, size_t c, size_t gamma);
void level_clear(level *lvl);
void level_print(level *lvl);

level* level_create_vstar   (size_t q, size_t c, size_t gamma);
level* level_create_vsk     (size_t q, size_t c, size_t gamma, size_t s, size_t k);
level* level_create_vc      (size_t q, size_t c, size_t gamma);
level* level_create_vhatsok (size_t q, size_t c, size_t gamma, size_t s, size_t o, size_t k, circuit *circ);

#endif
