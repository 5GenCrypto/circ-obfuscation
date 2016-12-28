#include "../util.h"

#include <acirc.h>
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    size_t sym_number; // k \in [c]
    size_t bit_number; // j \in [\ell]
} sym_id;

typedef sym_id (*input_chunker)   (size_t id, size_t ninputs, size_t nsyms);
typedef size_t (*reverse_chunker) (sym_id sym, size_t ninputs, size_t nsyms);

static sym_id chunker_in_order(size_t id, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    size_t k = floor((double)id / (double) chunksize);
    size_t j = id % chunksize;
    sym_id sym = { k, j };
    return sym;
}

static size_t rchunker_in_order(sym_id sym, size_t ninputs, size_t nsyms)
{
    size_t chunksize = ceil((double) ninputs / (double) nsyms);
    size_t id = sym.sym_number * chunksize + sym.bit_number;
    assert(id < ninputs);
    return id;
}

/* static sym_id chunker_mod(input_id id, size_t ninputs, size_t nsyms) */
/* { */
/*     (void) ninputs; */
/*     size_t k = id % nsyms; */
/*     size_t j = floor((double) id / (double) nsyms); */
/*     sym_id sym = { k, j }; */
/*     return sym; */
/* } */

/* static input_id rchunker_mod(sym_id sym, size_t ninputs, size_t nsyms) */
/* { */
/*     input_id id = sym.sym_number + sym.bit_number * nsyms; */
/*     assert((size_t) id < ninputs); */
/*     return id; */
/* } */

/* void test_chunker ( */
/*     input_chunker chunker, */
/*     reverse_chunker rchunker, */
/*     size_t ninputs, */
/*     size_t nsyms */
/* ) { */
/*     for (int j = 0; j < 100; j++) { */
/*         input_id id = 1 + (rand() % (ninputs-1)); */
/*         sym_id sym = chunker(id, ninputs, nsyms); */
/*         input_id id_ = rchunker(sym, ninputs, nsyms); */
/*         assert(id == id_); */
/*     } */
/* } */

/* void test_chunker_rand (input_chunker chunker, reverse_chunker rchunker) */
/* { */
/*     srand(time(NULL)); */
/*     for (int i = 0; i < 100; i++) { */
/*         size_t ninputs = 1 + (rand() % 1000); */
/*         size_t nsyms   = 1 + (rand() % 100); */
/*         test_chunker(chunker, rchunker, ninputs, nsyms); */
/*     } */
/* } */

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

static void
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

/* static size_t */
/* td(acircref ref, acirc *c, size_t nsyms, input_chunker chunker) */
/* { */
/*     size_t deg[nsyms+1]; */
/*     type_degree(deg, ref, c, nsyms, chunker); */
/*     size_t res = 0; */
/*     for (size_t i = 0; i < nsyms+1; i++) */
/*         res += deg[i]; */
/*     return res; */
/* } */

