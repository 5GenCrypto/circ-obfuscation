#pragma once

#include "../circ_params.h"
#include "../index_set.h"
#include "../mmap.h"
#include "../util.h"

struct obf_params_t {
    circ_params_t cp;
    size_t npowers;
    size_t nlevels;
    size_t nswitches;
};

/* [ Z W₁ ... Wₙ X₁ ... Xₙ Y ] */
#define IX_Z(ix)        (ix)->pows[0]
#define IX_W(ix, cp, i) (ix)->pows[1 + (i)]
#define IX_X(ix, cp, i) (ix)->pows[1 + (cp)->nslots + (i)]
#define IX_Y(ix, cp)    (ix)->pows[1 + (cp)->nslots + (cp)->nslots]

PRIVATE size_t obf_params_nzs(const circ_params_t *cp);
PRIVATE index_set * obf_params_new_toplevel(const circ_params_t *const cp, size_t nzs);
PRIVATE size_t obf_num_encodings(const circ_params_t *cp, size_t npowers);
