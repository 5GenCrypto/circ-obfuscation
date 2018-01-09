#pragma once

#include "../circ_params.h"
#include "../index_set.h"
#include "../mmap.h"

#define IX_Z(ix) (ix)->pows[0]
#define IX_W(ix, cp, i) (ix)->pows[1 + (i)]
#define IX_X(ix, cp, i) (ix)->pows[1 + (cp)->n + (i)]

size_t mife_params_nzs(const circ_params_t *cp);
index_set * mife_params_new_toplevel(const circ_params_t *const cp, size_t nzs);

