#pragma once

#include "circ_params.h"
#include "index_set.h"
#include "mmap.h"

struct obf_params_t {
    circ_params_t cp;
    size_t npowers;
};

#define IX_Z(ix) (ix)->pows[0]
#define IX_W(ix, cp, i) (ix)->pows[1 + (i)]
#define IX_X(ix, cp, i) (ix)->pows[1 + (cp)->n + (i)]

size_t mife_params_nzs(const circ_params_t *cp);
index_set * mife_params_new_toplevel(const circ_params_t *const cp, size_t nzs);
size_t mife_params_num_encodings_setup(const circ_params_t *cp);
size_t mife_params_num_encodings_encrypt(const circ_params_t *cp, size_t slot, size_t npowers);
