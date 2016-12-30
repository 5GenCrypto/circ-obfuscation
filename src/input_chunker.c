#include "input_chunker.h"
#include "util.h"

#include <assert.h>
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
    size_t id = sym.sym_number * chunksize + sym.bit_number;
    assert(id < ninputs);
    return id;
}

static void
type_degree_helper(size_t *rop, acircref ref, const acirc *c, size_t nsyms,
                   input_chunker chunker, bool *seen, size_t **memo)
{
    if (seen[ref]) {
        for (size_t i = 0; i < nsyms+1; i++)
            rop[i] = memo[ref][i];
    } else {
        const acirc_operation op = c->gates[ref].op;
        switch (op) {
        case OP_INPUT: {
            sym_id sym = chunker(c->gates[ref].args[0], c->ninputs, nsyms);
            memset(rop, '\0', sizeof(size_t) * (nsyms+1));
            assert(sym.sym_number < nsyms);
            rop[sym.sym_number] = 1;
            break;
        }
        case OP_CONST:
            memset(rop, '\0', sizeof(size_t) * (nsyms+1));
            rop[nsyms] = 1;
            break;
        case OP_ADD: case OP_SUB: case OP_MUL: {
            size_t xtype[nsyms+1];
            size_t ytype[nsyms+1];
            type_degree_helper(xtype, c->gates[ref].args[0], c, nsyms, chunker, seen, memo);
            type_degree_helper(ytype, c->gates[ref].args[1], c, nsyms, chunker, seen, memo);
            const bool eq = array_eq(xtype, ytype, nsyms + 1);
            if (eq && (op == OP_ADD || op == OP_SUB)) {
                for (size_t i = 0; i < nsyms+1; i++)
                    rop[i] = xtype[i];
            } else { // types unequal or op == MUL
                array_add(rop, xtype, ytype, nsyms+1);
            }
            break;
        }
        default:
            abort();
        }

        seen[ref] = true;
        for (size_t i = 0; i < nsyms+1; i++)
            memo[ref][i] = rop[i];
    }
}

void
type_degree(size_t *rop, acircref ref, const acirc *c, size_t nsyms, input_chunker chunker)
{
    bool    seen[c->nrefs];
    size_t *memo[c->nrefs];

    for (size_t i = 0; i < c->nrefs; i++) {
        seen[i] = false;
        memo[i] = my_calloc(nsyms+1, sizeof(size_t));
    }

    type_degree_helper(rop, ref, c, nsyms, chunker, seen, memo);

    for (size_t i = 0; i < c->nrefs; i++)
        free(memo[i]);
}
