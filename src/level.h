#ifndef __SRC_LEVEL_H__
#define __SRC_LEVEL_H__

#include "circuit.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t q;         // number of symbols in sigma
    size_t c;         // number of inputs
    size_t gamma;     // number of outputs
    uint32_t **types; // (gamma x q)-size array
    uint32_t m;       // max type degree in circuit over all output wires
    uint32_t d;       // degree(U) + c + 1
} params;

void params_init (
    params *lp,
    size_t q,
    size_t c,
    size_t gamma,
    circuit *circ,
    size_t (*input_chunker)(size_t input_num, size_t ninputs, size_t nsyms)
);

void params_clear (params *lp);

typedef struct {
    uint32_t **mat;
    uint32_t *vec;
    params *lp;
} level;

void level_init  (level *lvl, params *lp);
void level_clear (level *lvl);
void level_print (level *lvl);

void level_set_vstar   (level *lvl);
void level_set_vsk     (level *lvl, size_t s, size_t k);
void level_set_vc      (level *lvl);
void level_set_vhatsok (level *lvl, size_t s, size_t o, size_t k);
void level_set_vhato   (level *lvl, size_t o);
void level_set_vbaro   (level *lvl, size_t o);
void level_set_vzt     (level *lvl);

#endif
