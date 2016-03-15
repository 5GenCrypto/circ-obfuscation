#ifndef __SRC_INPUT_CHUNKER_H__
#define __SRC_INPUT_CHUNKER_H__

#include <stddef.h>

// takes a particular input id, total number of inputs, total number of symbols
// returns which symbol this input id belongs to
typedef size_t (*input_chunker) (size_t input_num, size_t ninputs, size_t nsyms);

size_t chunker_in_order (size_t input_num, size_t ninputs, size_t nsyms);
size_t chunker_mod      (size_t input_num, size_t ninputs, size_t nsyms);

#endif
