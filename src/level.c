#include "level.h"

#include <stdlib.h>
#include <stdio.h>

level* level_create(size_t q, size_t c, size_t gamma)
{
    level *lvl = malloc(sizeof(level));
    uint32_t **mat = malloc((q+1) * sizeof(uint32_t*));
    for (int i = 0; i < q+1; i++) {
        mat[i] = calloc(c+2, sizeof(uint32_t));
    }
    uint32_t *vec = calloc(gamma, sizeof(uint32_t));
    lvl->q = q;
    lvl->c = c;
    lvl->gamma = gamma;
    lvl->mat = mat;
    lvl->vec = vec;
    return lvl;
}

void level_clear(level *lvl)
{
    for (int i = 0; i < lvl->q+1; i++) {
        free(lvl->mat[i]);
    }
    free(lvl->mat);
    free(lvl->vec);
    free(lvl);
}

void level_print(level *lvl)
{
    printf("[");
    for (int i = 0; i < lvl->q+1; i++) {
        if (i != 0)
            printf(",");
        printf("[");
        for (int j = 0; j < lvl->c+2; j++) {
            // print mat[i][j]
            printf("%d", lvl->mat[i][j]);
            if (j != lvl->c+2-1)
                printf(",");
        }
        printf("]\n");
    }
    printf("],[");
    for (int i = 0; i < lvl->gamma; i++) {
        printf("%d", lvl->vec[i]);
        if (i != lvl->gamma-1)
            printf(",");
    }
    printf("]\n");
}

level* level_create_vstar(size_t q, size_t c, size_t gamma)
{
    level *lvl = level_create(q, c, gamma);
    lvl->mat[q][c+1] = 1;
    return lvl;
}

level* level_create_vsk(size_t q, size_t c, size_t gamma, size_t s, size_t k)
{
    level *lvl = level_create(q, c, gamma);
    lvl->mat[k][s] = 1;
    return lvl;
}

level* level_create_vc(size_t q, size_t c, size_t gamma)
{
    level *lvl = level_create(q, c, gamma);
    for (int i = 0; i < q; i++) {
        lvl->mat[i][c] = 1;
    }
    return lvl;
}

level* level_create_vhatsok(size_t q, size_t c, size_t gamma, size_t s, size_t o, size_t k, circuit *circ)
{
    level *lvl = level_create(q, c, gamma);
    uint32_t t = xdegree(circ, circ->outref, k);
    for (int i = 0; i < q; i++) {
        if (i != s)
            lvl->mat[i][k] = t;
    }
    lvl->mat[q][k] = 1;
    lvl->vec[o] = 1;
    return lvl;
}

