#include "extra.h"
#include "obf_params.h"
#include "../util.h"

#include <acirc.h>
#include <assert.h>

typedef struct {
    const obf_params_t *op;
    mmap_polylog_switch_params **sparams;
} args_t;

typedef struct {
    index_set *ix;
    size_t level;
} encoding_t;

static void *
input_f(size_t ref, size_t i, void *args_)
{
    (void) ref; (void) i; (void) args_;
    encoding_t *enc;

    enc = calloc(1, sizeof enc[0]);
    enc->ix = NULL;
    enc->level = 0;
    return enc;
}

static void *
const_f(size_t ref, size_t i, long val, void *args_)
{
    (void) ref; (void) i; (void) val; (void) args_;
    encoding_t *enc;

    enc = calloc(1, sizeof enc[0]);
    enc->ix = NULL;
    enc->level = 0;
    return enc;
}

static void *
eval_f(size_t ref, acirc_op op, size_t xref, const void *x_, size_t yref, const void *y_, void *args_)
{
    (void) xref; (void) yref;
    args_t *args = args_;
    mmap_polylog_switch_params *sparams;
    const encoding_t *x = x_;
    const encoding_t *y = y_;
    encoding_t *rop;
    size_t level;

    rop = calloc(1, sizeof rop[0]);

    switch (op) {
    case ACIRC_OP_ADD:
    case ACIRC_OP_SUB:
    case ACIRC_OP_MUL:
        if (x->level != y->level) {
            sparams = &args->sparams[ref][0];
            sparams->source = x->level < y->level ? x->level : y->level;
            sparams->target = x->level < y->level ? y->level : x->level;
            printf("Creating boosting switch parameters [%lu: %lu -> %lu]\n",
                   ref, sparams->source, sparams->target);
            sparams->ix = NULL;
        }
        sparams = &args->sparams[ref][1];
        level = max(x->level, y->level);
        sparams->source = level;
        sparams->target = level + 1;
        sparams->ix = NULL;
        printf("Creating switch parameters [%lu: %lu -> %lu]\n",
               ref, sparams->source, sparams->target);
        rop->ix = NULL;
        rop->level = level + 1;
        break;
    }
    return rop;
}

static void *
output_f(size_t ref, size_t o, void *x_, void *args_)
{
    args_t *args = args_;
    mmap_polylog_switch_params *sparams;
    const acirc_t *circ = args->op->circ;
    const size_t ninputs = acirc_ninputs(circ);
    const encoding_t *x = x_;
    const size_t top = args->op->nlevels;

    /* Compute LHS */
    ref = acirc_nrefs(circ) + o * (ninputs + 2);
    if (x->level < top - 1) {
        sparams = &args->sparams[ref][0];
        sparams->source = x->level;
        sparams->target = top - 1;
        printf("Creating LHS boosting switch params [%lu: %lu -> %lu]\n",
               ref, sparams->source, sparams->target);
    }
    sparams = &args->sparams[ref][1];
    sparams->source = top - 1;
    sparams->target = top;
    printf("Creating LHS switch params [%lu: %lu -> %lu]\n",
           ref, sparams->source, sparams->target);

    ref++;

    /* Compute RHS */
    for (size_t i = 0; i < ninputs; ++i) {
        sparams = &args->sparams[ref + i][1];
        sparams->source = i;
        sparams->target = i + 1;
        printf("Creating RHS switch param [%lu: %lu -> %lu]\n",
               ref + i, sparams->source, sparams->target);
    }

    sparams = &args->sparams[ref + ninputs][1];
    sparams->source = min(top, ninputs);
    sparams->target = max(top, ninputs);
    if (sparams->source == sparams->target) {
        sparams->source = 0;
        sparams->target = 0;
    } else {
        printf("Creating sub switch param [%lu: %lu -> %lu]\n",
               ref + ninputs, sparams->source, sparams->target);
    }

    return NULL;
}

static void
free_f(void *x, void *args_)
{
    (void) args_;
    if (x)
        free(x);
}

static mmap_polylog_switch_params **
polylog_switch_params(const obf_params_t *op, size_t nzs)
{
    (void) nzs;
    mmap_polylog_switch_params **sparams;
    encoding_t **outputs;

    sparams = calloc(op->nswitches, sizeof sparams[0]);
    for (size_t i = 0; i < op->nswitches; ++i) {
        sparams[i] = calloc(2, sizeof sparams[i][0]);
    }
    args_t args = { .op = op, .sparams = sparams };
    outputs = (encoding_t **) acirc_traverse((acirc_t *) op->circ, input_f, const_f,
                                             eval_f, output_f, free_f,
                                             NULL, NULL, &args, 0);
    free(outputs);
    return sparams;
}

mmap_sk
polylog_secret_params_new(const sp_vtable *vt, const obf_params_t *op,
                          mmap_sk_params *p, mmap_sk_opt_params *o,
                          const mmap_params_t *params, size_t ncores,
                          aes_randstate_t rng)
{
    mmap_sk sk = NULL;
    o->is_polylog = true;
    o->polylog.nlevels = op->nlevels;
    o->polylog.nswitches = op->nswitches;
    o->polylog.sparams = polylog_switch_params(op, params->nzs);
    o->polylog.wordsize = op->wordsize;

    if ((sk = vt->mmap->sk->new(p, o, ncores, rng, g_verbose)) == NULL)
        goto cleanup;
cleanup:
    for (size_t i = 0; i < op->nswitches; ++i)
        free(o->polylog.sparams[i]);
    free(o->polylog.sparams);
    return sk;
}
