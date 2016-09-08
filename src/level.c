#include "level.h"
#include "util.h"

#include "input_chunker.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// level utils

level *
level_new(obf_params *p)
{
    level *lvl = calloc(1, sizeof(level));
    lvl->q = p->q;
    lvl->c = p->c;
    lvl->gamma = p->gamma;
    lvl->mat = lin_malloc((p->q+1) * sizeof(size_t*));
    for (size_t i = 0; i < p->q+1; i++) {
        lvl->mat[i] = lin_calloc(p->c+2, sizeof(size_t));
    }
    lvl->vec = lin_calloc(p->gamma, sizeof(size_t));
    return lvl;
}

// it is the user's responsibility to clear lvl
void
level_free(level *lvl)
{
    for (size_t i = 0; i < lvl->q+1; i++) {
        free(lvl->mat[i]);
    }
    free(lvl->mat);
    free(lvl->vec);
    free(lvl);
}

void level_print (level *lvl)
{
    printf("[");
    for (size_t i = 0; i < lvl->q+1; i++) {
        if (i != 0)
            printf(",");
        printf("[");
        for (size_t j = 0; j < lvl->c+2; j++) {
            // print mat[i][j]
            printf("%lu", lvl->mat[i][j]);
            if (j != lvl->c+2-1)
                printf(",");
        }
        printf("]\n");
    }
    printf("],[");
    for (size_t i = 0; i < lvl->gamma; i++) {
        printf("%lu", lvl->vec[i]);
        if (i != lvl->gamma-1)
            printf(",");
    }
    printf("]\n");
}

void level_set (level *rop, const level *lvl)
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

void level_add (level *rop, const level *x, const level *y)
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

void level_mul_ui (level *rop, level *op, int x)
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

void level_flatten (int *pows, const level *lvl)
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

int level_eq (level *x, level *y)
{
    for (size_t i = 0; i < x->q+1; i++) {
        for (size_t j = 0; j < x->c+2; j++) {
            if (x->mat[i][j] != y->mat[i][j])
                return 0;
        }
    }
    for (size_t i = 0; i < x->gamma; i++) {
        if (x->vec[i] != y->vec[i])
            return 0;
    }
    return 1;
}

// for testing whether we can use constrained addition, we dont consider the
// degree (bottom right corner of the level)
int level_eq_z (level *x, level *y)
{
    for (size_t i = 0; i < x->q+1; i++) {
        for (size_t j = 0; j < x->c+2; j++) {
            if (i == x->q && j == x->c+1)
                continue;
            if (x->mat[i][j] != y->mat[i][j])
                return 0;
        }
    }
    for (size_t i = 0; i < x->gamma; i++) {
        if (x->vec[i] != y->vec[i])
            return 0;
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// level creators

level* level_create_vstar (obf_params *p)
{
    level *lvl = level_new(p);
    lvl->mat[lvl->q][lvl->c+1] = 1;
    return lvl;
}

level* level_create_vks (obf_params *p, size_t k, size_t s)
{
    assert(s < p->q);
    assert(k < p->c);
    level *lvl = level_new(p);
    lvl->mat[s][k] = 1;
    return lvl;
}

level* level_create_vc (obf_params *p)
{
    level *lvl = level_new(p);
    for (size_t i = 0; i < lvl->q; i++) {
        lvl->mat[i][lvl->c] = 1;
    }
    return lvl;
}

level* level_create_vhatkso (obf_params *p, size_t k, size_t s, size_t o)
{
    assert(s < p->q);
    assert(o < p->gamma);
    assert(k < p->c);
    level *lvl = level_new(p);
    for (size_t i = 0; i < lvl->q; i++) {
        if (i != s)
            lvl->mat[i][k] = p->types[o][k];
    }
    lvl->mat[p->q][k] = 1;
    lvl->vec[o] = 1;
    return lvl;
}

level* level_create_vhato (obf_params *p, size_t o)
{
    assert(o < p->gamma);
    level *lvl = level_new(p);
    for (size_t i = 0; i < lvl->q; i++) {
        for (size_t j = 0; j < lvl->c+1; j++) {
            lvl->mat[i][j] = p->M - p->types[o][j];
        }
    }
    lvl->mat[lvl->q][lvl->c] = 1;
    for (size_t i = 0; i < lvl->gamma; i++) {
        if (i != o)
            lvl->vec[i] = lvl->c;
    }
    return lvl;
}

level* level_create_vbaro (obf_params *p, size_t o)
{
    assert(o < p->gamma);
    level *lvl = level_new(p);
    for (size_t i = 0; i < lvl->q; i++) {
        lvl->mat[i][lvl->c+1] = 1;
    }
    return lvl;
}

level* level_create_vzt(obf_params *p)
{
    level *lvl = level_new(p);
    for (size_t i = 0; i < p->q + 1; i++) {
        for (size_t j = 0; j < p->c+1; j++) {
            if (i < p->q)
                lvl->mat[i][j] = p->M;
            else
                lvl->mat[i][j] = 1;
        }
        if (i < p->q)
            lvl->mat[i][p->c+1] = 1;
        else
            lvl->mat[i][p->c+1] = p->D;
    }
    for (size_t i = 0; i < lvl->gamma; i++) {
        lvl->vec[i] = lvl->c;
    }
    return lvl;
}

////////////////////////////////////////////////////////////////////////////////

void level_write (FILE *const fp, const level *lvl)
{
    ulong_write(fp, lvl->q);
    (void) PUT_SPACE(fp);
    ulong_write(fp, lvl->c);
    (void) PUT_SPACE(fp);
    ulong_write(fp, lvl->gamma);
    (void) PUT_SPACE(fp);
    for (size_t i = 0; i < lvl->q+1; i++) {
        for (size_t j = 0; j < lvl->c+2; j++) {
            ulong_write(fp, lvl->mat[i][j]);
            (void) PUT_SPACE(fp);
        }
    }
    (void) PUT_SPACE(fp);
    for (size_t i = 0; i < lvl->gamma; i++) {
        ulong_write(fp, lvl->vec[i]);
        (void) PUT_SPACE(fp);
    }
}

void level_read (level *lvl, FILE *const fp)
{
    ulong_read(&lvl->q, fp);
    GET_SPACE(fp);
    ulong_read(&lvl->c, fp);
    GET_SPACE(fp);
    ulong_read(&lvl->gamma, fp);
    GET_SPACE(fp);
    lvl->mat = lin_malloc((lvl->q+1) * sizeof(size_t*));
    for (size_t i = 0; i < lvl->q+1; i++) {
        lvl->mat[i] = lin_calloc(lvl->c+3, sizeof(size_t));
        for (size_t j = 0; j < lvl->c+2; j++) {
            ulong_read(&lvl->mat[i][j], fp);
            GET_SPACE(fp);
        }
    }
    GET_SPACE(fp);
    lvl->vec = lin_calloc(lvl->gamma, sizeof(size_t));
    for (size_t i = 0; i < lvl->gamma; i++) {
        ulong_read(&lvl->vec[i], fp);
        GET_SPACE(fp);
    }
}
