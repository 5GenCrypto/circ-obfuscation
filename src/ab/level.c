#include "obf_params.h"
#include "../util.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    size_t nrows;
    size_t ncols;
    size_t **mat;
} level;

static level *
level_new(const obf_params_t *op)
{
    level *lvl = calloc(1, sizeof(level));
    lvl->nrows = op->simple ? 3 : 4;
    lvl->ncols = op->n + op->m + 1;
    lvl->mat = my_malloc(lvl->nrows * sizeof(size_t*));
    for (size_t i = 0; i < lvl->nrows; i++) {
        lvl->mat[i] = my_calloc(lvl->ncols, sizeof(size_t));
    }
    return lvl;
}

static void
level_free(level *lvl)
{
    for (size_t i = 0; i < lvl->nrows; i++) {
        free(lvl->mat[i]);
    }
    free(lvl->mat);
    free(lvl);
}

static void
level_fprint(FILE *fp, const level *lvl)
{
    fprintf(fp, "[");
    for (size_t i = 0; i < lvl->nrows; i++) {
        if (i != 0)
            fprintf(fp, " ");
        fprintf(fp, "[");
        for (size_t j = 0; j < lvl->ncols; j++) {
            fprintf(fp, "%lu", lvl->mat[i][j]);
            if (j != lvl->ncols - 1)
                fprintf(fp, ",");
        }
        fprintf(fp, "]");
        if (i == lvl->nrows - 1)
            fprintf(fp, "]");
        /* fprintf(fp, "\n"); */
    }
    fprintf(fp, "\n");
}

static void
level_set(level *rop, const level *lvl)
{
    for (size_t i = 0; i < lvl->nrows; i++) {
        for (size_t j = 0; j < lvl->ncols; j++) {
            rop->mat[i][j] = lvl->mat[i][j];
        }
    }
}

static void
level_add(level *rop, const level *x, const level *y)
{
    for (size_t i = 0; i < rop->nrows; i++) {
        for (size_t j = 0; j < rop->ncols; j++) {
            rop->mat[i][j] = x->mat[i][j] + y->mat[i][j];
        }
    }
}

static void
level_mul_ui(level *rop, level *op, int x)
{
    for (size_t i = 0; i < rop->nrows; i++) {
        for (size_t j = 0; j < rop->ncols; j++) {
            rop->mat[i][j] = op->mat[i][j] * x;
        }
    }
}

static void
level_flatten(int *pows, const level *lvl)
{
    int z = 0;
    for (size_t i = 0; i < lvl->nrows; i++) {
        for (size_t j = 0; j < lvl->ncols; j++) {
            pows[z++] = lvl->mat[i][j];
        }
    }
}

static int
level_eq (const level *x, const level *y)
{
    for (size_t i = 0; i < x->nrows; i++) {
        for (size_t j = 0; j < x->ncols; j++) {
            if (x->mat[i][j] != y->mat[i][j])
                return 0;
        }
    }
    return 1;
}

static level *
level_create_v_ib(const obf_params_t *op, size_t i, bool b)
{
    level *lvl = level_new(op);
    lvl->mat[1][i] = 1;
    if (b)
        lvl->mat[0][i] = 1;
    else
        lvl->mat[2][i] = 1;
    return lvl;
}

static level *
level_create_v_i(const obf_params_t *op, size_t i)
{
    level *lvl = level_new(op);
    lvl->mat[0][i] = 1;
    lvl->mat[1][i] = 1;
    lvl->mat[2][i] = 1;
    return lvl;
}

static level *
level_create_v_hat_ib_o(const obf_params_t *op, size_t i, bool b, size_t o)
{
    level *lvl = level_new(op);
    if (b)
        lvl->mat[2][i] = op->types[o][i];
    else
        lvl->mat[0][i] = op->types[o][i];
    if (!op->simple) {
        lvl->mat[3][i] = 1;
        /* lvl->mat[lvl->nrows-1][lvl->ncols-1] = op->types[o][i]; */
    }
    return lvl;
}

/* static level * */
/* level_create_v_0(const obf_params_t *op) */
/* { */
/*     level *lvl = level_new(op); */
/*     lvl->mat[0][lvl->ncols-1] = 1; */
/*     lvl->mat[1][lvl->ncols-1] = 1; */
/*     lvl->mat[2][lvl->ncols-1] = 1; */
/*     return lvl; */
/* } */

static level *
level_create_v_hat_o(const obf_params_t *op, size_t o)
{
    level *lvl = level_new(op);
    for (size_t i = 0; i < lvl->nrows; i++) {
        for (size_t j = 0; j < lvl->ncols - 1; j++) {
            lvl->mat[i][j] = op->M - op->types[o][j];
            if (!op->simple && i == lvl->nrows - 1) {
                lvl->mat[i][j] = 0;
                
            }
        }
    }
    lvl->mat[0][lvl->ncols-1] = 1;
    lvl->mat[1][lvl->ncols-1] = 1;
    lvl->mat[2][lvl->ncols-1] = 1;
    return lvl;
}

static level *
level_create_v_star(const obf_params_t *op)
{
    level *lvl = level_new(op);
    if (!op->simple) {
        lvl->mat[3][lvl->ncols-1] = 1;
    }
    return lvl;
}

static level *
level_create_vzt(const obf_params_t *op)
{
    level *lvl = level_new(op);
    if (op->simple) {
        for (size_t i = 0; i < lvl->nrows; i++) {
            for (size_t j = 0; j < lvl->ncols; j++) {
                if (j < lvl->ncols - 1)
                    lvl->mat[i][j] = op->M;
                else
                    lvl->mat[i][j] = 1;
            }
        }
    } else {
        for (size_t i = 0; i < lvl->nrows; i++) {
            for (size_t j = 0; j < lvl->ncols; j++) {
                if (j < lvl->ncols - 1)
                    lvl->mat[i][j] = op->M;
                else
                    lvl->mat[i][j] = 1;

                if (i == lvl->nrows - 1) {
                    if (j < op->n) {
                        lvl->mat[i][j] = 1;
                    } else if (j < op->n + op->m) {
                        lvl->mat[i][j] = 0;
                    } else {
                        lvl->mat[i][j] = op->D;
                    }
                }
            }
        }
    }
    return lvl;
}

static void
level_fwrite(const level *lvl, FILE *fp)
{
    assert(lvl->nrows && lvl->ncols);
    size_t_fwrite(lvl->nrows, fp);
    PUT_SPACE(fp);
    size_t_fwrite(lvl->ncols, fp);
    PUT_SPACE(fp);
    for (size_t i = 0; i < lvl->nrows; i++) {
        for (size_t j = 0; j < lvl->ncols; j++) {
            ulong_fwrite(lvl->mat[i][j], fp);
            PUT_SPACE(fp);
        }
    }
}

static void
level_fread(level *lvl, FILE *fp)
{
    size_t_fread(&lvl->nrows, fp);
    GET_SPACE(fp);
    size_t_fread(&lvl->ncols, fp);
    GET_SPACE(fp);
    assert(lvl->nrows && lvl->ncols);
    lvl->mat = my_calloc(lvl->nrows, sizeof(size_t*));
    for (size_t i = 0; i < lvl->nrows; i++) {
        lvl->mat[i] = my_calloc(lvl->ncols, sizeof(size_t));
        for (size_t j = 0; j < lvl->ncols; j++) {
            ulong_fread(&lvl->mat[i][j], fp);
            GET_SPACE(fp);
        }
    }
}
