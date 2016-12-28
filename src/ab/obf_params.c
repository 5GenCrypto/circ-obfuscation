#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

static void
type_degree_helper(size_t *rop, acircref ref, const acirc *c, size_t len,
                   bool *seen, size_t **memo)
{
    if (seen[ref]) {
        for (size_t i = 0; i < len; i++)
            rop[i] = memo[ref][i];
        return;
    }

    const acirc_operation op = c->gates[ref].op;

    if (op == OP_INPUT) {
        rop[c->gates[ref].args[0]] = 1;
    } else if (op == OP_CONST) {
        rop[c->ninputs + c->gates[ref].args[0]] = 1;
    } else {
        size_t *xtype = my_calloc(len, sizeof(size_t));
        size_t *ytype = my_calloc(len, sizeof(size_t));

        type_degree_helper(xtype, c->gates[ref].args[0], c, len, seen, memo);
        type_degree_helper(ytype, c->gates[ref].args[1], c, len, seen, memo);

        for (size_t i = 0; i < len; i++)
            rop[i] = xtype[i] + ytype[i];

        free(xtype);
        free(ytype);
    }

    seen[ref] = true;

    for (size_t i = 0; i < len; i++)
        memo[ref][i] = rop[i];
}

static void
type_degree(size_t *rop, acircref ref, const acirc *c, size_t len)
{
    bool    seen[c->nrefs];
    size_t *memo[c->nrefs];

    for (size_t i = 0; i < c->nrefs; i++) {
        seen[i] = false;
        memo[i] = my_calloc(len, sizeof(size_t));
    }

    type_degree_helper(rop, ref, c, len, seen, memo);

    for (size_t i = 0; i < c->nrefs; i++)
        free(memo[i]);
}

static obf_params_t *
_op_new(acirc *circ, void *vparams)
{
    ab_obf_params_t *const params = vparams;
    obf_params_t *p = calloc(1, sizeof(obf_params_t));
    p->n = circ->ninputs;
    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    p->M = 0;
    p->types = my_calloc(p->gamma, sizeof(size_t *));
    for (size_t o = 0; o < p->gamma; o++) {
        p->types[o] = my_calloc(p->n + p->m + 1, sizeof(size_t));
        type_degree(p->types[o], circ->outrefs[o], circ, p->n + p->m);
        for (size_t k = 0; k < p->n + p->m; k++) {
            if (p->types[o][k] > p->M) {
                p->M = p->types[o][k];
            }
        }
    }
    p->d = acirc_max_degree(circ);
    p->D = p->d + p->n;
    p->nslots = params->simple ? 2 : (p->n + 2);
    if (g_verbose) {
        fprintf(stderr, "Obfuscation parameters:\n");
        fprintf(stderr, "* # inputs:  %lu\n", p->n);
        fprintf(stderr, "* # consts:  %lu\n", p->m);
        fprintf(stderr, "* # outputs: %lu\n", p->gamma);
        fprintf(stderr, "* M:         %lu\n", p->M);
        fprintf(stderr, "* degree:    %lu\n", p->d);
        fprintf(stderr, "* D:         %lu\n", p->D);
    }

    p->circ = circ;
    p->simple = params->simple;
    return p;
}

static void
_op_free(obf_params_t *p)
{
    for (size_t i = 0; i < p->gamma; i++) {
        free(p->types[i]);
    }
    free(p->types);
    free(p);
}

static int
_op_fwrite(const obf_params_t *params, FILE *fp)
{
    size_t_fwrite(params->n, fp);
    size_t_fwrite(params->m, fp);
    size_t_fwrite(params->gamma, fp);
    size_t_fwrite(params->M, fp);
    size_t_fwrite(params->d, fp);
    size_t_fwrite(params->D, fp);
    size_t_fwrite(params->nslots, fp);
    for (size_t o = 0; o < params->gamma; ++o) {
        for (size_t k = 0; k < params->m + params->m + 1; ++k) {
            size_t_fwrite(params->types[o][k], fp);
        }
    }
    bool_fwrite(params->simple, fp);
    return 0;
}

static int
_op_fread(obf_params_t *params, FILE *fp)
{
    size_t_fread(&params->n, fp);
    size_t_fread(&params->m, fp);
    size_t_fread(&params->gamma, fp);
    size_t_fread(&params->M, fp);
    size_t_fread(&params->d, fp);
    size_t_fread(&params->D, fp);
    size_t_fread(&params->nslots, fp);
    params->types = my_calloc(params->gamma, sizeof(size_t *));
    for (size_t o = 0; o < params->gamma; ++o) {
        params->types[o] = my_calloc(params->n + params->m + 1, sizeof(size_t));
        for (size_t k = 0; k < params->m + params->m + 1; ++k) {
            size_t_fread(&params->types[o][k], fp);
        }
    }
    bool_fread(&params->simple, fp);
    return 0;
}

op_vtable ab_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
    .fwrite = _op_fwrite,
    .fread = _op_fread,
};
