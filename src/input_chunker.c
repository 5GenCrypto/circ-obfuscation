#include "input_chunker.h"
#include "util.h"

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
    input_id id = sym.sym_number * chunksize + sym.bit_number;
    assert(id < ninputs);
    return id;
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
    input_id id = sym.sym_number + sym.bit_number * nsyms;
    assert(id < ninputs);
    return id;
}

void test_chunker (
    input_chunker chunker,
    reverse_chunker rchunker,
    size_t ninputs,
    size_t nsyms
) {
    for (int j = 0; j < 100; j++) {
        input_id id = 1 + (rand() % (ninputs-1));
        sym_id sym = chunker(id, ninputs, nsyms);
        input_id id_ = rchunker(sym, ninputs, nsyms);
        assert(id == id_);
    }
}

void test_chunker_rand (input_chunker chunker, reverse_chunker rchunker)
{
    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        size_t ninputs = 1 + (rand() % 1000);
        size_t nsyms   = 1 + (rand() % 100);
        test_chunker(chunker, rchunker, ninputs, nsyms);
    }
}

size_t td (circref ref, circuit *c, size_t nsyms, input_chunker chunker)
{
    size_t deg[nsyms+1];
    type_degree(deg, ref, c, nsyms, chunker);
    size_t res = 0;
    for (int i = 0; i < nsyms+1; i++)
        res += deg[i];
    return res;
}

void type_degree (
    size_t *rop,
    circref ref,
    circuit *c,
    size_t nsyms,
    input_chunker chunker
) {
    operation op = c->ops[ref];
    if (op == XINPUT) {
        sym_id sym = chunker(c->args[ref][0], c->ninputs, nsyms);
        rop[sym.sym_number] = 1;
        return;
    }
    if (op == YINPUT) {
        rop[nsyms] = 1;
        return;
    }

    size_t *xtype = lin_calloc(nsyms+1, sizeof(size_t));
    size_t *ytype = lin_calloc(nsyms+1, sizeof(size_t));

    type_degree(xtype, c->args[ref][0], c, nsyms, chunker);
    type_degree(ytype, c->args[ref][1], c, nsyms, chunker);

    int types_eq = array_eq_ui(xtype, ytype, nsyms + 1);
    int degs_eq = degree(c, c->args[ref][0]) == degree(c, c->args[ref][1]);

    // NOTE: added degree equality to avoid this bug:
    //
    //      w1 <- x + y
    //      w2 <- x * y
    //      w3 <- w1 - w2
    //      output w3
    //
    // w3 will not have top-level index because the subtraction
    // is not constrained subtraction due to the difference in
    // degree.
    if (degs_eq && types_eq && ((op == ADD) || (op == SUB))) {
        for (size_t i = 0; i < nsyms+1; i++)
            rop[i] = xtype[i];
    } else { // types unequal or op == MUL
        for (size_t i = 0; i < nsyms+1; i++)
            rop[i] = xtype[i] + ytype[i];
    }

    free(xtype);
    free(ytype);
}

