#pragma once

#include "../circ_params.h"
#include "../index_set.h"
#include "../mmap.h"

struct obf_params_t {
    size_t npowers;
};

#define IX_Z(ix) (ix)->pows[0]
#define IX_W(ix, circ, i) (ix)->pows[1 + (i)]
#define IX_X(ix, circ, i) (ix)->pows[1 + acirc_nslots(circ) + (i)]

const acirc_t *pp_circ(const public_params *pp);
size_t mife_params_nzs(const acirc_t *circ);
index_set * mife_params_new_toplevel(const acirc_t *circ);
