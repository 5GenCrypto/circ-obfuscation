#include "level.h"
#include "util.h"

#include "input_chunker.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// level utils

level *
level_new(obf_params *op)
{
    level *lvl = calloc(1, sizeof(level));
    lvl->nrows = op->simple ? 3 : 4;
    lvl->ncols = op->n + op->m + 1;
    lvl->mat = lin_malloc(lvl->nrows * sizeof(size_t*));
    for (size_t i = 0; i < lvl->nrows; i++) {
        lvl->mat[i] = lin_calloc(lvl->ncols, sizeof(size_t));
    }
    return lvl;
}

void
level_free(level *lvl)
{
    for (size_t i = 0; i < lvl->nrows; i++) {
        free(lvl->mat[i]);
    }
    free(lvl->mat);
    free(lvl);
}

void
level_print(level *lvl)
{
    printf("[");
    for (size_t i = 0; i < lvl->nrows; i++) {
        if (i != 0)
            printf(" ");
        printf("[");
        for (size_t j = 0; j < lvl->ncols; j++) {
            printf("%lu", lvl->mat[i][j]);
            if (j != lvl->ncols - 1)
                printf(",");
        }
        printf("]");
        if (i == lvl->nrows - 1)
            printf("]");
        printf("\n");
    }
}

void
level_set(level *rop, const level *lvl)
{
    for (size_t i = 0; i < lvl->nrows; i++) {
        for (size_t j = 0; j < lvl->ncols; j++) {
            rop->mat[i][j] = lvl->mat[i][j];
        }
    }
}

void
level_add(level *rop, const level *x, const level *y)
{
    for (size_t i = 0; i < rop->nrows; i++) {
        for (size_t j = 0; j < rop->ncols; j++) {
            rop->mat[i][j] = x->mat[i][j] + y->mat[i][j];
        }
    }
}

void
level_mul_ui(level *rop, level *op, int x)
{
    for (size_t i = 0; i < rop->nrows; i++) {
        for (size_t j = 0; j < rop->ncols; j++) {
            rop->mat[i][j] = op->mat[i][j] * x;
        }
    }
}

void
level_flatten(int *pows, const level *lvl)
{
    int z = 0;
    for (size_t i = 0; i < lvl->nrows; i++) {
        for (size_t j = 0; j < lvl->ncols; j++) {
            pows[z++] = lvl->mat[i][j];
        }
    }
}

int level_eq (level *x, level *y)
{
    for (size_t i = 0; i < x->nrows; i++) {
        for (size_t j = 0; j < x->ncols; j++) {
            if (x->mat[i][j] != y->mat[i][j])
                return 0;
        }
    }
    return 1;
}

level *
level_create_v_ib(obf_params *op, size_t i, bool b)
{
    level *lvl = level_new(op);
    lvl->mat[1][i] = 1;
    if (b)
        lvl->mat[0][i] = 1;
    else
        lvl->mat[2][i] = 1;
    return lvl;
}

level *
level_create_v_i(obf_params *op, size_t i)
{
    level *lvl = level_new(op);
    lvl->mat[0][i] = 1;
    lvl->mat[1][i] = 1;
    lvl->mat[2][i] = 1;
    return lvl;
}

level *
level_create_v_hat_ib_o(obf_params *op, size_t i, bool b, size_t o)
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

level *
level_create_v_0(obf_params *op)
{
    level *lvl = level_new(op);
    lvl->mat[0][lvl->ncols-1] = 1;
    lvl->mat[1][lvl->ncols-1] = 1;
    lvl->mat[2][lvl->ncols-1] = 1;
    return lvl;
}

level *
level_create_v_hat_o(obf_params *op, size_t o)
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

level *
level_create_v_star(obf_params *op)
{
    level *lvl = level_new(op);
    if (!op->simple) {
        lvl->mat[3][lvl->ncols-1] = 1;
    }
    return lvl;
}

level *
level_create_vzt(obf_params *op)
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

////////////////////////////////////////////////////////////////////////////////

void level_write (FILE *const fp, const level *lvl)
{
    ulong_write(fp, lvl->nrows);
    (void) PUT_SPACE(fp);
    ulong_write(fp, lvl->ncols);
    (void) PUT_SPACE(fp);
    for (size_t i = 0; i < lvl->nrows; i++) {
        for (size_t j = 0; j < lvl->ncols; j++) {
            ulong_write(fp, lvl->mat[i][j]);
            (void) PUT_SPACE(fp);
        }
    }
}

void level_read (level *lvl, FILE *const fp)
{
    ulong_read(&lvl->nrows, fp);
    GET_SPACE(fp);
    ulong_read(&lvl->ncols, fp);
    GET_SPACE(fp);
    lvl->mat = lin_malloc((lvl->nrows) * sizeof(size_t*));
    for (size_t i = 0; i < lvl->nrows; i++) {
        lvl->mat[i] = lin_calloc(lvl->ncols, sizeof(size_t));
        for (size_t j = 0; j < lvl->ncols; j++) {
            ulong_read(&lvl->mat[i][j], fp);
            GET_SPACE(fp);
        }
    }
}
