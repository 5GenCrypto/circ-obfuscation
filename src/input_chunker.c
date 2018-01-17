#include "input_chunker.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <string.h>

sym_id chunker_in_order(size_t id, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    size_t k = floor((double)id / (double) chunksize);
    size_t j = id % chunksize;
    sym_id sym = { k, j };
    return sym;
}

size_t rchunker_in_order(sym_id sym, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    return sym.sym_number * chunksize + sym.bit_number;
}

long *
get_input_syms(const long *inputs, size_t ninputs, reverse_chunker rchunker,
               size_t c, size_t ell, size_t q, bool sigma)
{
    long *input_syms;

    if ((input_syms = calloc(c, sizeof input_syms[0])) == NULL)
        return NULL;
    for (size_t i = 0; i < c; i++) {
        input_syms[i] = 0;
        for (size_t j = 0; j < ell; j++) {
            const sym_id sym = { i, j };
            const size_t k = rchunker(sym, ninputs, c);
            if (sigma)
                input_syms[i] += inputs[k] * j;
            else
                input_syms[i] += inputs[k] << j;
        }
        if ((size_t) input_syms[i] >= q) {
            fprintf(stderr, "error: invalid input (%ld > |Σ|)\n", input_syms[i]);
            free(input_syms);
            return NULL;
        }
    }
    return input_syms;
}
