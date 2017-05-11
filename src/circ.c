#include "circ.h"
#include "util.h"

#include <gmp.h>
#include <stdlib.h>
#include <string.h>

int
circ_eval(acirc *circ, const mpz_t *xs, const mpz_t *ys, const mpz_t modulus,
          mpz_t *cache)
{
    /* Assumes the circuit is topologically sorted */
    for (size_t ref = 0; ref < acirc_nrefs(circ); ++ref) {
        const acirc_gate_t *gate = &circ->gates.gates[ref];
        const acirc_operation op = gate->op;
        switch (op) {
        case OP_INPUT:
            mpz_set(cache[ref], xs[gate->args[0]]);
            break;
        case OP_CONST:
            mpz_set(cache[ref], ys[gate->args[0]]);
            break;
        case OP_ADD: case OP_SUB: case OP_MUL:
            /* Assumes that gates have exactly two inputs */
            if (op == OP_ADD) {
                mpz_add(cache[ref], cache[gate->args[0]], cache[gate->args[1]]);
                mpz_mod(cache[ref], cache[ref], modulus);
            } else if (op == OP_SUB) {
                mpz_sub(cache[ref], cache[gate->args[0]], cache[gate->args[1]]);
                mpz_mod(cache[ref], cache[ref], modulus);
            } else if (op == OP_MUL) {
                mpz_mul(cache[ref], cache[gate->args[0]], cache[gate->args[1]]);
                mpz_mod(cache[ref], cache[ref], modulus);
            }
            break;
        default:
            abort();
        }
    }
    return OK;
}
