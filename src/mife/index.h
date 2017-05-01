#pragma once

#include <stdlib.h>

typedef struct {
    int *pows;
    size_t nzs;
} index_set_t;

#define IX_X(ix, op, i, j)  (ix)->pows[]
