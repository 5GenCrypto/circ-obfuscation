#include "../mmap.h"
#include "obf_params.h"
#include "obfuscator.h"
#include "input_chunker.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

static obf_params_t *
_op_new(const acirc *const circ, int flags)
{
    const bool simple = (flags & AB_FLAG_SIMPLE) > 0;
    obf_params_t *p = calloc(1, sizeof(obf_params_t));
    p->n = circ->ninputs;
    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    p->M = 0;
    p->types = my_calloc(p->gamma, sizeof(size_t *));
    for (size_t o = 0; o < p->gamma; o++) {
        p->types[o] = my_calloc(p->n + p->m + 1, sizeof(size_t));
        type_degree(p->types[o], circ->outrefs[o], circ, p->n + p->m, chunker_in_order);
        for (size_t k = 0; k < p->n + p->m; k++) {
            if (p->types[o][k] > p->M) {
                p->M = p->types[o][k];
            }
        }
    }
    p->d = acirc_max_degree(circ);
    p->D = p->d + p->n;
    p->nslots = simple ? 2 : (p->n + 2);

    p->circ = circ;
    p->chunker = chunker_in_order;
    p->rchunker = rchunker_in_order;
    p->simple = simple;
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
_op_fwrite(const obf_params_t *const params, FILE *const fp)
{
    size_t_fwrite(params->n, fp);
    PUT_SPACE(fp);
    size_t_fwrite(params->m, fp);
    PUT_SPACE(fp);
    size_t_fwrite(params->gamma, fp);
    PUT_SPACE(fp);
    size_t_fwrite(params->M, fp);
    PUT_SPACE(fp);
    size_t_fwrite(params->d, fp);
    PUT_SPACE(fp);
    size_t_fwrite(params->D, fp);
    PUT_SPACE(fp);
    size_t_fwrite(params->nslots, fp);
    PUT_SPACE(fp);
    for (size_t o = 0; o < params->gamma; ++o) {
        for (size_t k = 0; k < params->m + params->m + 1; ++k) {
            size_t_fwrite(params->types[o][k], fp);
            PUT_SPACE(fp);
        }
    }
    bool_fwrite(params->simple, fp);
    PUT_SPACE(fp);
    return 0;
}

static int
_op_fread(obf_params_t *const params, FILE *const fp)
{
    size_t_fread(&params->n, fp);
    GET_SPACE(fp);
    size_t_fread(&params->m, fp);
    GET_SPACE(fp);
    size_t_fread(&params->gamma, fp);
    GET_SPACE(fp);
    size_t_fread(&params->M, fp);
    GET_SPACE(fp);
    size_t_fread(&params->d, fp);
    GET_SPACE(fp);
    size_t_fread(&params->D, fp);
    GET_SPACE(fp);
    size_t_fread(&params->nslots, fp);
    GET_SPACE(fp);
    params->types = my_calloc(params->gamma, sizeof(size_t *));
    for (size_t o = 0; o < params->gamma; ++o) {
        params->types[o] = my_calloc(params->n + params->m + 1, sizeof(size_t));
        for (size_t k = 0; k < params->m + params->m + 1; ++k) {
            size_t_fread(&params->types[o][k], fp);
            PUT_SPACE(fp);
        }
    }
    bool_fread(&params->simple, fp);
    GET_SPACE(fp);
    params->chunker = chunker_in_order;
    params->rchunker = rchunker_in_order;
    return 0;
}

op_vtable ab_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
    .fwrite = _op_fwrite,
    .fread = _op_fread,
};
