#include "input_chunker.h"

#include <math.h>

size_t chunker_in_order (size_t input_num, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    return floor((double)input_num / (double) chunksize);
}

size_t chunker_mod (size_t input_num, size_t ninputs, size_t nsyms)
{
    return input_num % nsyms;
}

size_t rchunker_mod (size_t k, size_t l, size_t ninputs, size_t nsyms)
{
    return k + l * nsyms;
}
