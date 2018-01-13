#include "extra.h"
#include "obf_params.h"
#include "../util.h"

#include <acirc.h>
#include <assert.h>

size_t
polylog_nlevels(obf_params_t *op)
{
    return op->nlevels;
}

size_t
polylog_nswitches(obf_params_t *op)
{
    return op->nswitches;
}

typedef struct {
    obf_params_t *op;
    mmap_polylog_switch_params *sparams;
    size_t count;
} args_t;

static void *
input_f(size_t i, void *args_)
{
    (void) i; (void) args_;
    return (void *) 0;
}

static void *
const_f(size_t i, long val, void *args_)
{
    (void) i; (void) val; (void) args_;
    return (void *) 0;
}

static void *
eval_f(acirc_op op, const void *x_, const void *y_, void *args_)
{
    args_t *args = args_;
    mmap_polylog_switch_params *sparams;
    const size_t x = (size_t) x_;
    const size_t y = (size_t) y_;
    size_t level;

    switch (op) {
    case ACIRC_OP_MUL:
        if (x != y) {
            fprintf(stderr, "unequal levels\n");
            abort();
        }
        sparams = &args->sparams[args->count++];
        sparams->source = x;
        sparams->target = level = x + 1;
        sparams->ix = NULL;
        break;
    case ACIRC_OP_ADD:
    case ACIRC_OP_SUB:
        if (x != y) {
            fprintf(stderr, "unequal levels\n");
            abort();
        }
        level = x;
        break;
    }
    return (void *) level;
}

static void *
output_f(size_t o, void *x_, void *args_)
{
    (void) o;
    args_t *args = args_;
    mmap_polylog_switch_params *sparams;
    const size_t ninputs = acirc_ninputs(args->op->cp.circ);
    const size_t x = (size_t) x_;

    /* Compute LHS */
    sparams = &args->sparams[args->count++];
    sparams->source = x;
    sparams->target = args->op->nlevels;

    /* Compute RHS */
    for (size_t i = 0; i < ninputs; ++i) {
        sparams = &args->sparams[args->count++];
        sparams->source = i;
        sparams->target = i + 1;
    }

    return (void *) 0;
}

mmap_polylog_switch_params *
polylog_switch_params(obf_params_t *op, size_t nzs)
{
    (void) nzs;
    mmap_polylog_switch_params *sparams;
    long *outputs;

    sparams = calloc(op->nswitches, sizeof sparams[0]);
    args_t args = { .op = op, .sparams = sparams, .count = 0 };
    outputs = (long *) acirc_traverse(op->cp.circ, input_f, const_f, eval_f,
                                      output_f, NULL, &args, 0);
    free(outputs);
    printf("%lu %lu\n", args.count, op->nswitches);
    assert(args.count == op->nswitches);
    return sparams;
}
