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

typedef struct {
    index_set *ix;
    size_t level;
} encoding_t;

mmap_polylog_switch_params *
switch_param_next(mmap_polylog_switch_params *sparams, size_t *count)
{
    return realloc(sparams, (++(*count)) * sizeof sparams[0]);
}

/* static void */
/* _raise_encoding(const obfuscation *obf, encoding *x, encoding *u, size_t diff) */
/* { */
/*     while (diff > 0) { */
/*         /\* size_t p = 0; *\/ */
/*         /\* while (((size_t) 1 << (p+1)) <= diff && (p+1) < obf->npowers) *\/ */
/*         /\*     p++; *\/ */
/*         /\* encoding_mul(obf->enc_vt, obf->pp_vt, x, x, us[p], obf->pp); *\/ */
/*         /\* diff -= (1 << p); *\/ */
/*         encoding_mul(obf->enc_vt, obf->pp_vt, x, x, u, obf->pp, 0); /\* XXX *\/ */
/*         diff -= 1; */
/*     } */
/* } */

static int
raise_encoding(encoding_t *x, const index_set *target, args_t *args)
{
    const circ_params_t *const cp = &args->op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    mmap_polylog_switch_params *sparams;
    index_set *ix;
    size_t diff;

    if ((ix = index_set_difference(target, x->ix)) == NULL)
        return ERR;
    for (size_t i = 0; i < ninputs; ++i) {
        diff = IX_X(ix, cp, i);
        if (diff > 0) {
            printf("RAISING ENCODING\n");
            args->sparams = switch_param_next(args->sparams, &args->count);
            sparams = &args->sparams[args->count];
            sparams->source = x->level;
            sparams->target = x->level + 1;
            sparams->ix = x->ix->pows;
        }
    }
    index_set_free(ix);
    return OK;
}

static int
raise_encodings(encoding_t *x, encoding_t *y, args_t *args)
{
    index_set *ix;
    int ret = ERR;

    if ((ix = index_set_union(x->ix, y->ix)) == NULL)
        goto cleanup;
    if (raise_encoding(x, ix, args) == ERR)
        goto cleanup;
    if (raise_encoding(y, ix, args) == ERR)
        goto cleanup;
    ret = OK;
cleanup:
    if (ix)
        index_set_free(ix);
    return ret;
}

static void *
input_f(size_t i, void *args_)
{
    (void) args_;
    /* const args_t *args = args_; */
    /* const circ_params_t *cp = &args->op->cp; */
    encoding_t *enc;

    enc = calloc(1, sizeof enc[0]);
    enc->ix = NULL; /* index_set_new(obf_params_nzs(cp)); */
    /* IX_X(enc->ix, cp, i) = 1; */
    enc->level = 0;
    printf("----------------------------\n");
    printf("INPUT %lu\n", i);
    index_set_print(enc->ix);
    printf("LEVEL: %lu\n", enc->level);
    printf("----------------------------\n");
    return enc;
}

static void *
const_f(size_t i, long val, void *args_)
{
    (void) i; (void) val; (void) args_;
    /* const args_t *args = args_; */
    /* const circ_params_t *cp = &args->op->cp; */
    encoding_t *enc;

    enc = calloc(1, sizeof enc[0]);
    enc->ix = NULL; /* index_set_new(obf_params_nzs(cp)); */
    /* IX_Y(enc->ix, cp) = 1; */
    enc->level = 0;
    printf("----------------------------\n");
    printf("CONST %lu\n", i);
    index_set_print(enc->ix);
    printf("LEVEL: %lu\n", enc->level);
    printf("----------------------------\n");
    return enc;
}

static void *
eval_f(acirc_op op, const void *x_, const void *y_, void *args_)
{
    args_t *args = args_;
    mmap_polylog_switch_params *sparams;
    const encoding_t *x = x_;
    const encoding_t *y = y_;
    encoding_t *rop;

    rop = calloc(1, sizeof rop[0]);

    printf("OP = %u\n", op);

    switch (op) {
    case ACIRC_OP_MUL:
        printf("----------------------------\n");
        printf("X\n");
        index_set_print(x->ix);
        printf("LEVEL: %lu\n", x->level);
        printf("Y\n");
        index_set_print(y->ix);
        printf("LEVEL: %lu\n", y->level);
        printf("----------------------------\n");
        if (x->level != y->level) {
            fprintf(stderr, "unequal levels\n");
            abort();
        }
        args->sparams = switch_param_next(args->sparams, &args->count);
        sparams = &args->sparams[args->count];
        sparams->source = x->level;
        sparams->target = x->level + 1;
        sparams->ix = NULL; /* x->ix->pows; */
        rop->ix = NULL; /* index_set_copy(x->ix); */
        rop->level = sparams->target;
        printf("----------------------------\n");
        printf("MUL\n");
        index_set_print(rop->ix);
        printf("LEVEL: %lu\n", rop->level);
        printf("----------------------------\n");
        break;
    case ACIRC_OP_ADD:
    case ACIRC_OP_SUB:
        printf("----------------------------\n");
        printf("X\n");
        index_set_print(x->ix);
        printf("LEVEL: %lu\n", x->level);
        printf("Y\n");
        index_set_print(y->ix);
        printf("LEVEL: %lu\n", y->level);
        printf("----------------------------\n");
        if (!index_set_eq(x->ix, y->ix))
            raise_encodings(x, y, args);
        sparams = &args->sparams[args->count];
        rop->ix = NULL; /* index_set_copy(sparams->ix); */
        rop->level = sparams->target;
        printf("----------------------------\n");
        printf("ADD/SUB\n");
        index_set_print(rop->ix);
        printf("LEVEL: %lu\n", rop->level);
        printf("----------------------------\n");
        break;
    }
    return rop;
}

static void *
output_f(size_t o, void *x_, void *args_)
{
    (void) o;
    args_t *args = args_;
    mmap_polylog_switch_params *sparams;
    const size_t ninputs = acirc_ninputs(args->op->cp.circ);
    const encoding_t *x = x_;

    /* Compute LHS */
    args->sparams = switch_param_next(args->sparams, &args->count);
    sparams = &args->sparams[args->count];
    sparams->source = x->level;
    sparams->target = x->level + 1;

    /* Compute RHS */
    for (size_t i = 0; i < ninputs; ++i) {
        args->sparams = switch_param_next(args->sparams, &args->count);
        sparams = &args->sparams[args->count];
        sparams->source = i;
        sparams->target = i + 1;
    }

    return NULL;
}

mmap_polylog_switch_params *
polylog_switch_params(obf_params_t *op, size_t nzs)
{
    (void) nzs;
    mmap_polylog_switch_params *sparams;
    encoding_t **outputs;

    sparams = calloc(1, sizeof sparams[0]);
    args_t args = { .op = op, .sparams = sparams, .count = 0 };
    outputs = (encoding_t **) acirc_traverse(op->cp.circ, input_f, const_f,
                                             eval_f, output_f, NULL, &args, 0);
    free(outputs);
    assert(args.count == op->nswitches);
    return sparams;
}
