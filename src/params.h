#ifndef __SRC_PARAMS_H__
#define __SRC_PARAMS_H__

#include "circuit.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t q;         // number of symbols in sigma
    size_t c;         // number of inputs
    size_t gamma;     // number of outputs
    uint32_t **types; // (gamma x q)-size array
    uint32_t m;       // max type degree in circuit over all output wires
} params;

void params_init  (params *p, circuit *circ, input_chunker chunker, size_t nsyms);
void params_clear (params *p);

#endif
