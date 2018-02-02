#pragma once

#include "../circ_params.h"
#include "../index_set.h"
#include "../mmap.h"
#include "../util.h"

struct obf_params_t {
    acirc_t *circ;
    size_t nlevels;
    size_t nswitches;
    size_t wordsize;
};

/* [ Z W₁ ... Wₙ X₁ ... Xₙ Y ] */
#define IX_Z(ix)          (ix)->pows[0]
#define IX_W(ix, circ, i) (ix)->pows[1 + (i)]
#define IX_X(ix, circ, i) (ix)->pows[1 + acirc_nslots(circ) + (i)]
#define IX_Y(ix, circ)    (ix)->pows[1 + 2 * acirc_nslots(circ)]

const acirc_t *pp_circ(const public_params *pp);
PRIVATE size_t obf_params_nzs(const acirc_t *circ);
PRIVATE index_set * obf_params_new_toplevel(const acirc_t *circ, size_t nzs);
PRIVATE size_t obf_num_encodings(const acirc_t *circ);
