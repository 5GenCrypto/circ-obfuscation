#include "circ.h"
#include "util.h"

#include <gmp.h>
#include <stdlib.h>
#include <string.h>
#include <threadpool.h>

/* typedef struct { */
/*     acirc_t *circ; */
/*     threadpool *pool; */
/*     acircref ref; */
/*     const mpz_t *xs, *ys; */
/*     ref_list *deps; */
/*     int *ready; */
/*     mpz_t *cache; */
/*     mpz_t modulus; */
/* } eval_args_t; */

/* static void */
/* eval_worker(void *vargs) */
/* { */
/*     eval_args_t *const eval = vargs; */
/*     acircref ref = eval->ref; */
/*     mpz_t *cache = eval->cache; */

/*     const acirc_gate_t *gate = &eval->circ->gates.gates[ref]; */
/*     const acirc_operation op = gate->op; */
/*     switch (op) { */
/*     case OP_INPUT: */
/*         mpz_set(cache[ref], eval->xs[gate->args[0]]); */
/*         break; */
/*     case OP_CONST: */
/*         mpz_set(cache[ref], eval->ys[gate->args[0]]); */
/*         break; */
/*     case OP_ADD: case OP_SUB: case OP_MUL: */
/*         /\* Assumes that gates have exactly two inputs *\/ */
/*         if (op == OP_ADD) { */
/*             mpz_add(cache[ref], cache[gate->args[0]], cache[gate->args[1]]); */
/*             mpz_mod(cache[ref], cache[ref], eval->modulus); */
/*         } else if (op == OP_SUB) { */
/*             mpz_sub(cache[ref], cache[gate->args[0]], cache[gate->args[1]]); */
/*             mpz_mod(cache[ref], cache[ref], eval->modulus); */
/*         } else if (op == OP_MUL) { */
/*             mpz_mul(cache[ref], cache[gate->args[0]], cache[gate->args[1]]); */
/*             mpz_mod(cache[ref], cache[ref], eval->modulus); */
/*         } */
/*         break; */
/*     default: */
/*         abort(); */
/*     } */

/*     ref_list_node *node = &eval->deps->refs[ref]; */
/*     for (size_t i = 0; i < node->cur; ++i) { */
/*         const int num = __sync_add_and_fetch(&eval->ready[node->refs[i]], 1); */
/*         if (num == 2) { */
/*             eval_args_t *neweval = my_calloc(1, sizeof neweval[0]); */
/*             memcpy(neweval, eval, sizeof neweval[0]); */
/*             mpz_init_set(neweval->modulus, eval->modulus); */
/*             neweval->ref = node->refs[i]; */
/*             threadpool_add_job(eval->pool, eval_worker, neweval); */
/*         } */
/*     } */

/*     mpz_clear(eval->modulus); */
/*     free(eval); */
/* } */

int
circ_eval(acirc_t *circ, mpz_t *xs, size_t n, mpz_t *ys, size_t m,
          mpz_t modulus, size_t nthreads)
{
    if (nthreads == 0) {
        acirc_eval_mpz(circ, xs, ys, modulus);
        /* /\* Assumes the circuit is topologically sorted *\/ */
        /* for (size_t ref = 0; ref < acirc_nrefs(circ); ++ref) { */
        /*     const acirc_gate_t *gate = &circ->gates.gates[ref]; */
        /*     const acirc_operation op = gate->op; */
        /*     switch (op) { */
        /*     case OP_INPUT: */
        /*         mpz_set(cache[ref], xs[gate->args[0]]); */
        /*         break; */
        /*     case OP_CONST: */
        /*         mpz_set(cache[ref], ys[gate->args[0]]); */
        /*         break; */
        /*     case OP_ADD: case OP_SUB: case OP_MUL: */
        /*         /\* Assumes that gates have exactly two inputs *\/ */
        /*         if (op == OP_ADD) { */
        /*             mpz_add(cache[ref], cache[gate->args[0]], cache[gate->args[1]]); */
        /*             mpz_mod(cache[ref], cache[ref], modulus); */
        /*         } else if (op == OP_SUB) { */
        /*             mpz_sub(cache[ref], cache[gate->args[0]], cache[gate->args[1]]); */
        /*             mpz_mod(cache[ref], cache[ref], modulus); */
        /*         } else if (op == OP_MUL) { */
        /*             mpz_mul(cache[ref], cache[gate->args[0]], cache[gate->args[1]]); */
        /*             mpz_mod(cache[ref], cache[ref], modulus); */
        /*         } */
        /*         break; */
        /*     default: */
        /*         abort(); */
        /*     } */
        /* } */
    } else {
        /* ref_list *deps = ref_list_new(circ); */
        /* int *ready = my_calloc(acirc_nrefs(circ), sizeof ready[0]); */
        /* threadpool *pool = threadpool_create(nthreads); */

        /* for (size_t ref = 0; ref < acirc_nrefs(circ); ++ref) { */
        /*     acirc_operation op = circ->gates.gates[ref].op; */
        /*     if (op != OP_INPUT && op != OP_CONST) */
        /*         continue; */
        /*     eval_args_t *eval = calloc(1, sizeof eval[0]); */
        /*     eval->circ = circ; */
        /*     eval->pool = pool; */
        /*     eval->ref = ref; */
        /*     eval->xs = xs; */
        /*     eval->ys = ys; */
        /*     eval->deps = deps; */
        /*     eval->ready = ready; */
        /*     eval->cache = cache; */
        /*     mpz_init_set(eval->modulus, modulus); */
        /*     threadpool_add_job(pool, eval_worker, eval); */
        /* } */

        /* threadpool_destroy(pool); */
        /* free(ready); */
        /* ref_list_free(deps, circ); */
    }
    return OK;
}
