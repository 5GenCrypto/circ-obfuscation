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

/* static void */
/* type_degree_helper(int *rop, acircref ref, const acirc *c, size_t nsyms, */
/*                    input_chunker chunker, bool *seen, int **memo) */
/* { */
/*     if (seen[ref]) { */
/*         for (size_t i = 0; i < nsyms+1; i++) */
/*             rop[i] = memo[ref][i]; */
/*     } else { */
/*         const acirc_operation op = c->gates.gates[ref].op; */
/*         switch (op) { */
/*         case OP_INPUT: { */
/*             sym_id sym = chunker(c->gates.gates[ref].args[0], c->ninputs, nsyms); */
/*             memset(rop, '\0', (nsyms+1) * sizeof rop[0]); */
/*             assert(sym.sym_number < nsyms); */
/*             rop[sym.sym_number] = 1; */
/*             break; */
/*         } */
/*         case OP_CONST: */
/*             memset(rop, '\0', (nsyms+1) * sizeof rop[0]); */
/*             rop[nsyms] = 1; */
/*             break; */
/*         case OP_ADD: case OP_SUB: case OP_MUL: { */
/*             int xtype[nsyms+1]; */
/*             int ytype[nsyms+1]; */
/*             type_degree_helper(xtype, c->gates.gates[ref].args[0], c, nsyms, chunker, seen, memo); */
/*             type_degree_helper(ytype, c->gates.gates[ref].args[1], c, nsyms, chunker, seen, memo); */
/*             const bool eq = array_eq(xtype, ytype, nsyms + 1); */
/*             if (eq && (op == OP_ADD || op == OP_SUB)) { */
/*                 for (size_t i = 0; i < nsyms+1; i++) */
/*                     rop[i] = xtype[i]; */
/*             } else { // types unequal or op == MUL */
/*                 array_add(rop, xtype, ytype, nsyms+1); */
/*             } */
/*             break; */
/*         } */
/*         default: */
/*             abort(); */
/*         } */

/*         seen[ref] = true; */
/*         for (size_t i = 0; i < nsyms+1; i++) */
/*             memo[ref][i] = rop[i]; */
/*     } */
/* } */

/* void */
/* type_degree(int *rop, acircref ref, const acirc *c, size_t nsyms, input_chunker chunker) */
/* { */
/*     bool seen[acirc_nrefs(c)]; */
/*     int *memo[acirc_nrefs(c)]; */

/*     for (size_t i = 0; i < acirc_nrefs(c); i++) { */
/*         seen[i] = false; */
/*         memo[i] = my_calloc(nsyms+1, sizeof memo[0][0]); */
/*     } */

/*     type_degree_helper(rop, ref, c, nsyms, chunker, seen, memo); */

/*     for (size_t i = 0; i < acirc_nrefs(c); i++) */
/*         free(memo[i]); */
/* } */

int *
get_input_syms(const int *inputs, size_t ninputs, reverse_chunker rchunker,
               size_t c, size_t ell, size_t q, bool sigma)
{
    int *input_syms = my_calloc(c, sizeof input_syms[0]);
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
            fprintf(stderr, "error: invalid input (%d > |Σ|)\n", input_syms[i]);
            free(input_syms);
            return NULL;
        }
    }
    return input_syms;
}
