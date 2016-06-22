#include "input_chunker.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

sym_id chunker_in_order (input_id id, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    size_t k = floor((double)id / (double) chunksize);
    size_t j = id % chunksize;
    sym_id sym = { k, j };
    return sym;
}

input_id rchunker_in_order (sym_id sym,  size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    return sym.sym_number * chunksize + sym.bit_number;
}

sym_id chunker_mod (input_id id, size_t ninputs, size_t nsyms)
{
    size_t k = id % nsyms;
    size_t j = floor((double) id / (double) nsyms);
    sym_id sym = { k, j };
    return sym;
}

input_id rchunker_mod (sym_id sym, size_t ninputs, size_t nsyms)
{
    return sym.sym_number + sym.bit_number * nsyms;
}

void test_chunker (input_chunker chunker, reverse_chunker rchunker)
{
    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        size_t ninputs = 1 + (rand() % 1000);
        size_t nsyms   = 1 + (rand() % 100);
        for (int j = 0; j < 100; j++) {
            input_id id = 1 + (rand() % (ninputs-1));
            sym_id sym = chunker(id, ninputs, nsyms);
            input_id id_ = rchunker(sym, ninputs, nsyms);
            assert(id == id_);
        }
    }
}
