#ifndef __SRC_INPUT_CHUNKER_H__
#define __SRC_INPUT_CHUNKER_H__

#include <stddef.h>

typedef size_t input_id;

typedef struct {
    size_t sym_number; // k \in [c]
    size_t bit_number; // j \in [\ell]
} sym_id;

// takes a particular input id, total number of inputs, total number of symbols
// returns which symbol this input id belongs to
typedef sym_id   (*input_chunker)   (input_id id, size_t ninputs, size_t nsyms);
typedef input_id (*reverse_chunker) (sym_id sym,  size_t ninputs, size_t nsyms);

sym_id   chunker_in_order  (input_id id, size_t ninputs, size_t nsyms);
input_id rchunker_in_order (sym_id sym,  size_t ninputs, size_t nsyms);

sym_id   chunker_mod  (input_id id, size_t ninputs, size_t nsyms);
input_id rchunker_mod (sym_id sym,  size_t ninputs, size_t nsyms);

void test_chunker (input_chunker chunker, reverse_chunker rchunker);

#endif
