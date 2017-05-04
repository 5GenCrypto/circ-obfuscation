#include "level.h"
#include "../util.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

level *
level_new(const circ_params_t *cp)
{
    const size_t q = array_max(cp->qs, cp->n);
    level *lvl = calloc(1, sizeof lvl[0]);
    lvl->q = q;
    lvl->c = cp->n;
    lvl->gamma = cp->m;
    lvl->mat = my_calloc(lvl->q + 1, sizeof lvl->mat[0]);
    for (size_t i = 0; i < lvl->q + 1; ++i)
        lvl->mat[i] = my_calloc(cp->n + 2, sizeof lvl->mat[i][0]);
    lvl->vec = my_calloc(cp->m, sizeof lvl->vec[0]);
    return lvl;
}

void
level_free(level *lvl)
{
    for (size_t i = 0; i < lvl->q + 1; ++i)
        free(lvl->mat[i]);
    free(lvl->mat);
    free(lvl->vec);
    free(lvl);
}

void
level_fprint(FILE *fp, const level *lvl)
{
    fprintf(fp, "[");
    for (size_t i = 0; i < lvl->q + 1; ++i) {
        if (i != 0)
            fprintf(fp, ",");
        fprintf(fp, "[");
        for (size_t j = 0; j < lvl->c + 2; ++j) {
            fprintf(fp, "%lu", lvl->mat[i][j]);
            if (j != lvl->c+2-1)
                fprintf(fp, ",");
        }
        fprintf(fp, "]");
    }
    fprintf(fp, "],[");
    for (size_t i = 0; i < lvl->gamma; ++i) {
        fprintf(fp, "%lu", lvl->vec[i]);
        if (i != lvl->gamma-1)
            fprintf(fp, ",");
    }
    fprintf(fp, "]\n");
}

void
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

void
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

void
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

int
level_flatten(int *pows, const level *lvl)
{
    int z = 0;
    for (size_t i = 0; i < lvl->q+1; i++) {
        for (size_t j = 0; j < lvl->c+2; j++) {
            if ((int) lvl->mat[i][j] < 0)
                return ERR;
            pows[z++] = (int) lvl->mat[i][j];
        }
    }
    for (size_t i = 0; i < lvl->gamma; i++) {
        if ((int) lvl->vec[i] < 0)
            return ERR;
        pows[z++] = (int) lvl->vec[i];
    }
    return OK;
}

bool
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

bool
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

level *
level_create_vstar(const circ_params_t *cp)
{
    level *lvl = level_new(cp);
    lvl->mat[lvl->q][lvl->c+1] = 1;
    return lvl;
}

level *
level_create_vks(const circ_params_t *cp, size_t k, size_t s)
{
    level *lvl = level_new(cp);
    lvl->mat[s][k] = 1;
    return lvl;
}

level *
level_create_vc(const circ_params_t *cp)
{
    level *lvl = level_new(cp);
    for (size_t i = 0; i < lvl->q; i++) {
        lvl->mat[i][lvl->c] = 1;
    }
    return lvl;
}

level *
level_create_vhatkso(const circ_params_t *cp, size_t k, size_t s, size_t o, int **types)
{
    level *lvl = level_new(cp);
    for (size_t i = 0; i < lvl->q; i++) {
        if (i != s)
            lvl->mat[i][k] = types[o][k];
    }
    lvl->mat[lvl->q][k] = 1;
    lvl->vec[o] = 1;
    return lvl;
}

level *
level_create_vhato(const circ_params_t *cp, size_t o, size_t M, int **types)
{
    level *lvl = level_new(cp);
    for (size_t i = 0; i < lvl->q; i++) {
        for (size_t j = 0; j < lvl->c+1; j++) {
            lvl->mat[i][j] = M - types[o][j];
        }
    }
    lvl->mat[lvl->q][lvl->c] = 1;
    for (size_t i = 0; i < lvl->gamma; i++) {
        if (i != o)
            lvl->vec[i] = lvl->c;
    }
    return lvl;
}

level *
level_create_vbaro(const circ_params_t *cp, size_t o)
{
    level *lvl = level_new(cp);
    for (size_t i = 0; i < lvl->q; i++) {
        lvl->mat[i][lvl->c+1] = 1;
    }
    return lvl;
}

level *
level_create_vzt(const circ_params_t *cp, size_t M, size_t D)
{
    level *lvl = level_new(cp);
    for (size_t i = 0; i < lvl->q + 1; i++) {
        for (size_t j = 0; j < lvl->c+1; j++) {
            if (i < lvl->q)
                lvl->mat[i][j] = M;
            else
                lvl->mat[i][j] = 1;
        }
        if (i < lvl->q)
            lvl->mat[i][lvl->c+1] = 1;
        else
            lvl->mat[i][lvl->c+1] = D;
    }
    for (size_t i = 0; i < lvl->gamma; i++) {
        lvl->vec[i] = lvl->c;
    }
    return lvl;
}

void
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

void
level_fread(level *lvl, FILE *fp)
{
    ulong_fread(&lvl->q, fp);
    ulong_fread(&lvl->c, fp);
    ulong_fread(&lvl->gamma, fp);
    lvl->mat = my_calloc(lvl->q + 1, sizeof lvl->mat[0]);
    for (size_t i = 0; i < lvl->q+1; i++) {
        lvl->mat[i] = my_calloc(lvl->c+3, sizeof lvl->mat[i][0]);
        for (size_t j = 0; j < lvl->c+2; j++) {
            ulong_fread(&lvl->mat[i][j], fp);
        }
    }
    lvl->vec = my_calloc(lvl->gamma, sizeof lvl->vec[0]);
    for (size_t i = 0; i < lvl->gamma; i++) {
        ulong_fread(&lvl->vec[i], fp);
    }
}
