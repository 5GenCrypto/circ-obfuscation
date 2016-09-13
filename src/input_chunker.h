#ifndef __SRC_INPUT_CHUNKER_H__
#define __SRC_INPUT_CHUNKER_H__

#include <acirc.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t sym_number; // k \in [c]
    size_t bit_number; // j \in [\ell]
} sym_id;

// takes a particular input id, total number of inputs, total number of symbols
// returns which symbol this input id belongs to
typedef sym_id   (*input_chunker)   (input_id id, size_t ninputs, size_t nsyms);
typedef input_id (*reverse_chunker) (sym_id sym,  size_t ninputs, size_t nsyms);

sym_id
chunker_in_order(input_id id, size_t ninputs, size_t nsyms);
input_id
rchunker_in_order(sym_id sym,  size_t ninputs, size_t nsyms);

void
type_degree(size_t *rop, acircref ref, const acirc *const c, size_t nsyms,
            input_chunker chunker);

#endif
