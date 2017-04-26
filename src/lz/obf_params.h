#pragma once

#include "circ_params.h"
#include "index_set.h"
#include "input_chunker.h"
#include "mmap.h"

#include <acirc.h>
#include <stddef.h>

struct obf_params_t {
    circ_params_t cp;
    bool sigma;
    size_t npowers;
    input_chunker chunker;
    reverse_chunker rchunker;
};

#define IX_Y(ix)           (ix)->pows[0]
#define IX_S(ix, cp, k, s) (ix)->pows[1 + (cp)->qs[0] * (k) + (s)]
#define IX_Z(ix, cp, k)    (ix)->pows[1 + (cp)->qs[0] * ((cp)->n - 1) + (k)]
#define IX_W(ix, cp, k)    (ix)->pows[1 + (1 + (cp)->qs[0]) * ((cp)->n - 1) + (k)]

size_t obf_params_nzs(const circ_params_t *cp);
index_set * obf_params_new_toplevel(const circ_params_t *cp, size_t nzs);
size_t obf_params_num_encodings(const obf_params_t *op);
