#include "input_chunker.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

sym_id
chunker_in_order(input_id id, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    size_t k = floor((double)id / (double) chunksize);
    size_t j = id % chunksize;
    sym_id sym = { k, j };
    return sym;
}

input_id
rchunker_in_order(sym_id sym,  size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    input_id id = sym.sym_number * chunksize + sym.bit_number;
    assert(id < ninputs);
    return id;
}

static void
type_degree_helper(size_t *rop, acircref ref, const acirc *const c, size_t nsyms,
                   input_chunker chunker, bool *seen, size_t **memo)
{
    if (seen[ref]) {
        for (size_t i = 0; i < nsyms; i++)
            rop[i] = memo[ref][i];
        return;
    }

    acirc_operation op = c->gates[ref].op;

    if (op == XINPUT) {
        sym_id sym = chunker(c->gates[ref].args[0], c->ninputs, nsyms);
        assert(sym.sym_number < nsyms);
        rop[sym.sym_number] = 1;
    } else if (op == YINPUT) {
        assert(c->ninputs + c->gates[ref].args[0] < nsyms);
        rop[c->ninputs + c->gates[ref].args[0]] = 1;
    } else {
        size_t *xtype = lin_calloc(nsyms, sizeof(size_t));
        size_t *ytype = lin_calloc(nsyms, sizeof(size_t));

        type_degree_helper(xtype, c->gates[ref].args[0], c, nsyms, chunker, seen, memo);
        type_degree_helper(ytype, c->gates[ref].args[1], c, nsyms, chunker, seen, memo);

        for (size_t i = 0; i < nsyms; i++)
            rop[i] = xtype[i] + ytype[i];

        free(xtype);
        free(ytype);
    }

    seen[ref] = true;

    for (size_t i = 0; i < nsyms; i++)
        memo[ref][i] = rop[i];
}

void
type_degree(size_t *rop, acircref ref, const acirc *const c, size_t nsyms,
            input_chunker chunker)
{
    bool    seen[c->nrefs];
    size_t *memo[c->nrefs];

    for (size_t i = 0; i < c->nrefs; i++) {
        seen[i] = false;
        memo[i] = lin_calloc(nsyms, sizeof(size_t));
    }

    type_degree_helper(rop, ref, c, nsyms, chunker, seen, memo);

    for (size_t i = 0; i < c->nrefs; i++)
        free(memo[i]);
}
