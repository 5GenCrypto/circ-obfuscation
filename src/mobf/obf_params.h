#pragma once

#include "../circ_params.h"
#include "../input_chunker.h"

#include <stddef.h>

struct obf_params_t {
    circ_params_t cp;
    int sigma;
    size_t npowers;
    input_chunker chunker;
    reverse_chunker rchunker;
};
