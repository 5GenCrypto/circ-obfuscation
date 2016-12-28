#include "obf_params.h"
#include "../util.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    size_t q;                   /* # symbols in alphabet */
    size_t c;                   /* # symbols in input */
    size_t gamma;
    size_t **mat;
    size_t *vec;
} level;

static level *
level_new(const obf_params_t *op)
{
    level *lvl = calloc(1, sizeof(level));
    lvl->q = op->q;
    lvl->c = op->c;
    lvl->gamma = op->gamma;
    lvl->mat = my_calloc(op->q+1, sizeof(size_t*));
    for (size_t i = 0; i < op->q+1; i++) {
        lvl->mat[i] = my_calloc(op->c+2, sizeof(size_t));
    }
    lvl->vec = my_calloc(op->gamma, sizeof(size_t));
    return lvl;
}

static void
level_free(level *lvl)
{
    for (size_t i = 0; i < lvl->q+1; i++) {
        free(lvl->mat[i]);
    }
    free(lvl->mat);
    free(lvl->vec);
    free(lvl);
}

static void
level_fprint(FILE *fp, const level *lvl)
{
    fprintf(fp, "[");
    for (size_t i = 0; i < lvl->q+1; i++) {
        if (i != 0)
            fprintf(fp, ",");
        fprintf(fp, "[");
        for (size_t j = 0; j < lvl->c+2; j++) {
            fprintf(fp, "%lu", lvl->mat[i][j]);
            if (j != lvl->c+2-1)
                fprintf(fp, ",");
        }
        fprintf(fp, "]");
    }
    fprintf(fp, "],[");
    for (size_t i = 0; i < lvl->gamma; i++) {
        fprintf(fp, "%lu", lvl->vec[i]);
        if (i != lvl->gamma-1)
            fprintf(fp, ",");
    }
    fprintf(fp, "]\n");
}

static void
level_set(level *rop, const level *lvl)
{
    for (size_t i = 0; i < lvl->q+1; i++) {
        for (size_t j = 0; j < lvl->c+2; j++) {
            rop->mat[i][j] = lvl->mat[i][j];
        }
    }
    for (size_t i = 0; i < lvl->gamma; i++) {
        rop->vec[i] = lvl->vec[i];
    }
}

static void
level_add(level *rop, const level *x, const level *y)
{
    for (size_t i = 0; i < rop->q+1; i++) {
        for (size_t j = 0; j < rop->c+2; j++) {
            rop->mat[i][j] = x->mat[i][j] + y->mat[i][j];
        }
    }
    for (size_t i = 0; i < rop->gamma; i++) {
        rop->vec[i] = x->vec[i] + y->vec[i];
    }
}

static void
level_mul_ui(level *rop, const level *op, int x)
{
    for (size_t i = 0; i < rop->q+1; i++) {
        for (size_t j = 0; j < rop->c+2; j++) {
            rop->mat[i][j] = op->mat[i][j] * x;
        }
    }
    for (size_t i = 0; i < rop->gamma; i++) {
        rop->vec[i] = op->vec[i] * x;
    }
}

static void
level_flatten(int *pows, const level *lvl)
{
    int z = 0;
    for (size_t i = 0; i < lvl->q+1; i++) {
        for (size_t j = 0; j < lvl->c+2; j++) {
            pows[z++] = lvl->mat[i][j];
        }
    }
    for (size_t i = 0; i < lvl->gamma; i++) {
        pows[z++] = lvl->vec[i];
    }
}

static bool
level_eq(const level *x, const level *y)
{
    for (size_t i = 0; i < x->q+1; i++) {
        for (size_t j = 0; j < x->c+2; j++) {
            if (x->mat[i][j] != y->mat[i][j])
                return false;
        }
    }
    for (size_t i = 0; i < x->gamma; i++) {
        if (x->vec[i] != y->vec[i])
            return false;
    }
    return true;
}

// for testing whether we can use constrained addition, we dont consider the
// degree (bottom right corner of the level)
static bool
level_eq_z(level *x, level *y)
{
    for (size_t i = 0; i < x->q+1; i++) {
        for (size_t j = 0; j < x->c+2; j++) {
            if (i == x->q && j == x->c+1)
                continue;
            if (x->mat[i][j] != y->mat[i][j])
                return false;
        }
    }
    for (size_t i = 0; i < x->gamma; i++) {
        if (x->vec[i] != y->vec[i])
            return false;
    }
    return true;
}

static level *
level_create_vstar(const obf_params_t *op)
{
    level *lvl = level_new(op);
    lvl->mat[lvl->q][lvl->c+1] = 1;
    return lvl;
}

static level *
level_create_vks(const obf_params_t *op, size_t k, size_t s)
{
    assert(s < op->q);
    assert(k < op->c);
    level *lvl = level_new(op);
    lvl->mat[s][k] = 1;
    return lvl;
}

static level *
level_create_vc(const obf_params_t *op)
{
    level *lvl = level_new(op);
    for (size_t i = 0; i < lvl->q; i++) {
        lvl->mat[i][lvl->c] = 1;
    }
    return lvl;
}

static level *
level_create_vhatkso(const obf_params_t *op, size_t k, size_t s, size_t o)
{
    assert(s < op->q);
    assert(o < op->gamma);
    assert(k < op->c);
    level *lvl = level_new(op);
    for (size_t i = 0; i < lvl->q; i++) {
        if (i != s)
            lvl->mat[i][k] = op->types[o][k];
    }
    lvl->mat[op->q][k] = 1;
    lvl->vec[o] = 1;
    return lvl;
}

static level *
level_create_vhato(const obf_params_t *op, size_t o)
{
    assert(o < op->gamma);
    level *lvl = level_new(op);
    for (size_t i = 0; i < lvl->q; i++) {
        for (size_t j = 0; j < lvl->c+1; j++) {
            lvl->mat[i][j] = op->M - op->types[o][j];
        }
    }
    lvl->mat[lvl->q][lvl->c] = 1;
    for (size_t i = 0; i < lvl->gamma; i++) {
        if (i != o)
            lvl->vec[i] = lvl->c;
    }
    return lvl;
}

static level *
level_create_vbaro(const obf_params_t *op, size_t o)
{
    assert(o < op->gamma);
    level *lvl = level_new(op);
    for (size_t i = 0; i < lvl->q; i++) {
        lvl->mat[i][lvl->c+1] = 1;
    }
    return lvl;
}

static level *
level_create_vzt(const obf_params_t *op)
{
    level *lvl = level_new(op);
    for (size_t i = 0; i < op->q + 1; i++) {
        for (size_t j = 0; j < op->c+1; j++) {
            if (i < op->q)
                lvl->mat[i][j] = op->M;
            else
                lvl->mat[i][j] = 1;
        }
        if (i < op->q)
            lvl->mat[i][op->c+1] = 1;
        else
            lvl->mat[i][op->c+1] = op->D;
    }
    for (size_t i = 0; i < lvl->gamma; i++) {
        lvl->vec[i] = lvl->c;
    }
    return lvl;
}

static void
level_fwrite(const level *lvl, FILE *fp)
{
    ulong_fwrite(lvl->q, fp);
    ulong_fwrite(lvl->c, fp);
    ulong_fwrite(lvl->gamma, fp);
    for (size_t i = 0; i < lvl->q+1; i++) {
        for (size_t j = 0; j < lvl->c+2; j++) {
            ulong_fwrite(lvl->mat[i][j], fp);
        }
    }
    for (size_t i = 0; i < lvl->gamma; i++) {
        ulong_fwrite(lvl->vec[i], fp);
    }
}

static void
level_fread(level *lvl, FILE *fp)
{
    ulong_fread(&lvl->q, fp);
    ulong_fread(&lvl->c, fp);
    ulong_fread(&lvl->gamma, fp);
    lvl->mat = my_calloc(lvl->q + 1, sizeof(size_t*));
    for (size_t i = 0; i < lvl->q+1; i++) {
        lvl->mat[i] = my_calloc(lvl->c+3, sizeof(size_t));
        for (size_t j = 0; j < lvl->c+2; j++) {
            ulong_fread(&lvl->mat[i][j], fp);
        }
    }
    lvl->vec = my_calloc(lvl->gamma, sizeof(size_t));
    for (size_t i = 0; i < lvl->gamma; i++) {
        ulong_fread(&lvl->vec[i], fp);
    }
}
