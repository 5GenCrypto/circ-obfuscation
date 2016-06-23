#include "level.h"
#include "util.h"

#include "input_chunker.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// level utils

void level_init (level *lvl, obf_params *p)
{
    lvl->mat = lin_malloc((p->q+1) * sizeof(uint32_t*));
    for (int i = 0; i < p->q+1; i++) {
        lvl->mat[i] = lin_calloc(p->c+2, sizeof(uint32_t));
    }
    lvl->vec = lin_calloc(p->gamma, sizeof(uint32_t));
    lvl->p = p;
}

// it is the user's responsibility to clear lvl->p
void level_clear (level *lvl)
{
    for (int i = 0; i < lvl->p->q+1; i++) {
        free(lvl->mat[i]);
    }
    free(lvl->mat);
    free(lvl->vec);
}

void level_destroy (level *lvl)
{
    level_clear(lvl);
    free(lvl);
}

void level_print (level *lvl)
{
    printf("[");
    for (int i = 0; i < lvl->p->q+1; i++) {
        if (i != 0)
            printf(",");
        printf("[");
        for (int j = 0; j < lvl->p->c+2; j++) {
            // print mat[i][j]
            printf("%d", lvl->mat[i][j]);
            if (j != lvl->p->c+2-1)
                printf(",");
        }
        printf("]\n");
    }
    printf("],[");
    for (int i = 0; i < lvl->p->gamma; i++) {
        printf("%d", lvl->vec[i]);
        if (i != lvl->p->gamma-1)
            printf(",");
    }
    printf("]\n");
}

void level_set (level *rop, const level *lvl)
{
    for (int i = 0; i < lvl->p->q+1; i++) {
        for (int j = 0; j < lvl->p->c+2; j++) {
            rop->mat[i][j] = lvl->mat[i][j];
        }
    }
    for (int i = 0; i < lvl->p->gamma; i++) {
        rop->vec[i] = lvl->vec[i];
    }
}

void level_add (level *rop, const level *x, const level *y)
{
    for (int i = 0; i < rop->p->q+1; i++) {
        for (int j = 0; j < rop->p->c+2; j++) {
            rop->mat[i][j] = x->mat[i][j] + y->mat[i][j];
        }
    }
    for (int i = 0; i < rop->p->gamma; i++) {
        rop->vec[i] = x->vec[i] + y->vec[i];
    }
}

void level_mul_ui (level *rop, level *op, int x)
{
    for (int i = 0; i < rop->p->q+1; i++) {
        for (int j = 0; j < rop->p->c+2; j++) {
            rop->mat[i][j] = op->mat[i][j] * x;
        }
    }
    for (int i = 0; i < rop->p->gamma; i++) {
        rop->vec[i] = op->vec[i] * x;
    }
}

int level_eq (level *x, level *y)
{
    for (int i = 0; i < x->p->q+1; i++) {
        for (int j = 0; j < x->p->c+2; j++) {
            if (x->mat[i][j] != y->mat[i][j])
                return 0;
        }
    }
    for (int i = 0; i < x->p->gamma; i++) {
        if (x->vec[i] != y->vec[i])
            return 0;
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// level creators

level* level_create_vstar (obf_params *p)
{
    level *lvl = lin_malloc(sizeof(level));
    level_init(lvl, p);
    lvl->mat[lvl->p->q][lvl->p->c+1] = 1;
    return lvl;
}

level* level_create_vks (obf_params *p, size_t k, size_t s)
{
    assert(s < p->q);
    assert(k < p->c);
    level *lvl = lin_malloc(sizeof(level));
    level_init(lvl, p);
    lvl->mat[s][k] = 1;
    return lvl;
}

level* level_create_vc (obf_params *p)
{
    level *lvl = lin_malloc(sizeof(level));
    level_init(lvl, p);
    for (int i = 0; i < lvl->p->q; i++) {
        lvl->mat[i][lvl->p->c] = 1;
    }
    return lvl;
}

level* level_create_vhatkso (obf_params *p, size_t k, size_t s, size_t o)
{
    assert(s < p->q);
    assert(o < p->gamma);
    assert(k < p->c);
    level *lvl = lin_malloc(sizeof(level));
    level_init(lvl, p);
    for (int i = 0; i < lvl->p->q; i++) {
        if (i != s)
            lvl->mat[i][k] = lvl->p->types[o][k];
    }
    lvl->mat[p->q][k] = 1;
    lvl->vec[o] = 1;
    return lvl;
}

level* level_create_vhato (obf_params *p, size_t o)
{
    assert(o < p->gamma);
    level *lvl = lin_malloc(sizeof(level));
    level_init(lvl, p);
    for (int i = 0; i < lvl->p->c+1; i++) {
        for (int j = 0; j < lvl->p->q; j++) {
            lvl->mat[i][j] = lvl->p->M - lvl->p->types[o][i];
        }
    }
    lvl->mat[lvl->p->c][lvl->p->q] = 1;
    for (int i = 0; i < lvl->p->gamma; i++) {
        if (i != o)
            lvl->vec[i] = lvl->p->c;
    }
    return lvl;
}

level* level_create_vbaro (obf_params *p, size_t o)
{
    assert(o < p->gamma);
    level *lvl = lin_malloc(sizeof(level));
    level_init(lvl, p);
    for (int i = 0; i < lvl->p->q; i++) {
        lvl->mat[i][lvl->p->c+1] = 1;
    }
    return lvl;
}
