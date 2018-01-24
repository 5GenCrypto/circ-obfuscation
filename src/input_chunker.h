#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t sym_number; // k \in [c]
    size_t bit_number; // j \in [\ell]
} sym_id;

typedef sym_id (*input_chunker)   (size_t id, size_t ninputs, size_t nsyms);
typedef size_t (*reverse_chunker) (sym_id sym, size_t ninputs, size_t nsyms);

sym_id chunker_in_order(size_t id, size_t ninputs, size_t nsyms);
size_t rchunker_in_order(sym_id sym, size_t ninputs, size_t nsyms);

long * get_input_syms(const long *inputs, size_t ninputs, reverse_chunker rchunker,
                      size_t c, const size_t *ds, const size_t *qs, const bool *sigmas);
