#include "level.h"

#include "input_chunker.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// level utils

void level_init (level *lvl, obf_params *p)
{
    lvl->mat = malloc((p->q+1) * sizeof(uint32_t*));
    for (int i = 0; i < p->q+1; i++) {
        lvl->mat[i] = calloc(p->c+2, sizeof(uint32_t));
    }
    lvl->vec = calloc(p->gamma, sizeof(uint32_t));
    lvl->p  = p;
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
    assert(rop->p == lvl->p);
    for (int i = 0; i < lvl->p->q+1; i++) {
        for (int j = 0; j < lvl->p->c+2; j++) {
            rop->mat[i][j] = lvl->mat[i][j];
        }
    }
    for (int i = 0; i < lvl->p->gamma; i++) {
        rop->vec[i] = lvl->vec[i];
    }
}

////////////////////////////////////////////////////////////////////////////////
// level creators

level* level_create_vstar (obf_params *p)
{
    level *lvl = malloc(sizeof(level*));
    lvl->mat[lvl->p->q][lvl->p->c+1] = 1;
}

level* level_create_vsk (obf_params *p, size_t s, size_t k)
{
    assert(s < p->q);
    assert(k < p->c);
    level *lvl = malloc(sizeof(level*));
    lvl->mat[k][s] = 1;
}

level* level_create_vc (obf_params *p)
{
    level *lvl = malloc(sizeof(level*));
    for (int i = 0; i < lvl->p->q; i++) {
        lvl->mat[i][lvl->p->c] = 1;
    }
}

level* level_create_vhatsok (obf_params *p, size_t s, size_t o, size_t k)
{
    assert(s < lvl->p->q);
    assert(o < lvl->p->gamma);
    assert(k < lvl->p->c);
    for (int i = 0; i < lvl->p->q; i++) {
        if (i != s)
            lvl->mat[i][k] = lvl->p->types[o][k];
    }
    lvl->mat[lvl->p->q][k] = 1;
    lvl->vec[o] = 1;
}

level* level_create_vhato (obf_params *p, size_t o)
{
    assert(o < lvl->p->gamma);
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
}

level* level_create_vbaro (obf_params *p, size_t o)
{
    assert(o < lvl->p->gamma);
    for (int i = 0; i < lvl->p->q; i++) {
        lvl->mat[i][lvl->p->c+1] = 1;
    }
}
