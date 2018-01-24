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
               size_t nsymbols, const size_t *ds, const size_t *qs,
               const bool *sigmas)
{
    (void) ninputs; (void) rchunker;
    long *input_syms;
    size_t k = 0;

    if ((input_syms = my_calloc(nsymbols, sizeof input_syms[0])) == NULL)
        return NULL;
    for (size_t i = 0; i < nsymbols; i++) {
        input_syms[i] = 0;
        for (size_t j = 0; j < ds[i]; j++) {
            /* const sym_id sym = { i, j }; */
            /* const size_t k = rchunker(sym, ninputs, nsymbols); */
            if (sigmas[i])
                input_syms[i] += inputs[k] * j;
            else
                input_syms[i] += inputs[k] << j;
            k++;
        }
        printf("%d\n", sigmas[i]);
        if ((size_t) input_syms[i] >= qs[i]) {
            fprintf(stderr, "error: invalid input for symbol %lu (%ld > %lu)\n",
                    i, input_syms[i], qs[i]);
            goto error;
        }
    }
    return input_syms;
error:
    if (input_syms)
        free(input_syms);
    return NULL;
}
