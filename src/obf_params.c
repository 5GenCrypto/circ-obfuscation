#include "obf_params.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

void
obf_params_init(obf_params_t *const p, const acirc *const circ,
                input_chunker chunker, reverse_chunker rchunker, bool simple)
{
    p->n = circ->ninputs;
    p->m = circ->nconsts;
    p->gamma = circ->noutputs;
    p->M = 0;
    p->types = lin_calloc(p->gamma, sizeof(size_t *));
    for (size_t o = 0; o < p->gamma; o++) {
        p->types[o] = lin_calloc(p->n + p->m + 1, sizeof(size_t));
        type_degree(p->types[o], circ->outrefs[o], circ, p->n + p->m, chunker);
        for (size_t k = 0; k < p->n + p->m; k++) {
            printf("%lu ", p->types[o][k]);
            if (p->types[o][k] > p->M) {
                p->M = p->types[o][k];
            }
        }
        printf("\n");
    }
    p->d = acirc_max_degree(circ);
    p->D = p->d + p->n;
    printf("%lu %lu\n", p->d, p->D);
    p->nslots = simple ? 2 : (p->n + 2);

    p->circ = circ;
    p->chunker = chunker;
    p->rchunker = rchunker;
    p->simple = simple;
}

int
obf_params_fwrite(const obf_params_t *const params, FILE *const fp)
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

int
obf_params_fread(obf_params_t *const params, FILE *const fp)
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
    params->types = lin_calloc(params->gamma, sizeof(size_t *));
    for (size_t o = 0; o < params->gamma; ++o) {
        params->types[o] = lin_calloc(params->n + params->m + 1, sizeof(size_t));
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


void
obf_params_clear(obf_params_t *p)
{
    for (size_t i = 0; i < p->gamma; i++) {
        free(p->types[i]);
    }
    free(p->types);
}
