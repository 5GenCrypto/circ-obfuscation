#include "level.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// level params

void params_init (
    params *lp,
    size_t q,
    size_t c,
    size_t gamma,
    circuit *circ,
    size_t (*input_chunker)(size_t input_num, size_t ninputs, size_t nsyms)
) {
    lp->q = q;
    lp->c = c;
    lp->gamma = gamma;
    lp->types = malloc(gamma * sizeof(uint32_t*));
    uint32_t max_t = 0;

    for (int i = 0; i < gamma; i++) {
        lp->types[i] = calloc(q, sizeof(uint32_t));
        type_degree(lp->types[i], circ->outrefs[i], circ, q, input_chunker);

        for (int j = 0; j < q; j++) {
            if (lp->types[i][j] > max_t)
                max_t = lp->types[i][j];
        }
    }
    lp->m = max_t;

    uint32_t max_d = 0;
    for (int i = 0; i < gamma; i++) {
        uint32_t d = degree(circ, circ->outrefs[i]);
        if (d > max_d)
            max_d = d;
    }
    lp->d = max_d + c + 1;
}

void params_clear (params *lp)
{
    for (int i = 0; i < lp->gamma; i++) {
        free(lp->types[i]);
    }
    free(lp->types);
}

////////////////////////////////////////////////////////////////////////////////
// level utils

void level_init (level *lvl, params *lp)
{
    lvl->mat = malloc((lp->q+1) * sizeof(uint32_t*));
    for (int i = 0; i < lp->q+1; i++) {
        lvl->mat[i] = calloc(lp->c+2, sizeof(uint32_t));
    }
    lvl->vec = calloc(lp->gamma, sizeof(uint32_t));
    lvl->lp  = lp;
}

// it is the user's responsibility to clear lvl->lp
void level_clear (level *lvl)
{
    for (int i = 0; i < lvl->lp->q+1; i++) {
        free(lvl->mat[i]);
    }
    free(lvl->mat);
    free(lvl->vec);
}

void level_print (level *lvl)
{
    printf("[");
    for (int i = 0; i < lvl->lp->q+1; i++) {
        if (i != 0)
            printf(",");
        printf("[");
        for (int j = 0; j < lvl->lp->c+2; j++) {
            // print mat[i][j]
            printf("%d", lvl->mat[i][j]);
            if (j != lvl->lp->c+2-1)
                printf(",");
        }
        printf("]\n");
    }
    printf("],[");
    for (int i = 0; i < lvl->lp->gamma; i++) {
        printf("%d", lvl->vec[i]);
        if (i != lvl->lp->gamma-1)
            printf(",");
    }
    printf("]\n");
}

////////////////////////////////////////////////////////////////////////////////
// level setters

void level_set_vstar (level *lvl)
{
    lvl->mat[lvl->lp->q][lvl->lp->c+1] = 1;
}

void level_set_vsk (level *lvl, size_t s, size_t k)
{
    assert(s < lvl->lp->q);
    assert(k < lvl->lp->c);
    lvl->mat[k][s] = 1;
}

void level_set_vc (level *lvl)
{
    for (int i = 0; i < lvl->lp->q; i++) {
        lvl->mat[i][lvl->lp->c] = 1;
    }
}

void level_set_vhatsok (level *lvl, size_t s, size_t o, size_t k)
{
    assert(s < lvl->lp->q);
    assert(o < lvl->lp->gamma);
    assert(k < lvl->lp->c);
    for (int i = 0; i < lvl->lp->q; i++) {
        if (i != s)
            lvl->mat[i][k] = lvl->lp->types[o][k];
    }
    lvl->mat[lvl->lp->q][k] = 1;
    lvl->vec[o] = 1;
}

void level_set_vhato (level *lvl, size_t o)
{
    assert(o < lvl->lp->gamma);
    for (int i = 0; i < lvl->lp->c+1; i++) {
        for (int j = 0; j < lvl->lp->q; j++) {
            lvl->mat[i][j] = lvl->lp->m - lvl->lp->types[o][i];
        }
    }
    lvl->mat[lvl->lp->c][lvl->lp->q] = 1;
    for (int i = 0; i < lvl->lp->gamma; i++) {
        if (i != o)
            lvl->vec[i] = lvl->lp->c;
    }
}

void level_set_vbaro (level *lvl, size_t o)
{
    assert(o < lvl->lp->gamma);
    for (int i = 0; i < lvl->lp->q; i++) {
        lvl->mat[i][lvl->lp->c+1] = 1;
    }
}

void level_set_vzt(level *lvl)
{
    for (int i = 0; i < lvl->lp->q; i++) {
        for (int j = 0; j < lvl->lp->c+1; j++) {
            lvl->mat[i][j] = lvl->lp->m;
        }
    }
    for (int i = 0; i < lvl->lp->c+1; i++) {
        lvl->mat[lvl->lp->q][i] = 1;
    }
    for (int i = 0; i < lvl->lp->q; i++) {
        lvl->mat[i][lvl->lp->c+1] = 1;
    }
    lvl->mat[lvl->lp->q][lvl->lp->c+1] = lvl->lp->d + lvl->lp->c + 1; // D = deg(U) + c + 1
    for (int i = 0; i < lvl->lp->gamma; i++) {
        lvl->vec[i] = lvl->lp->c;
    }
}
