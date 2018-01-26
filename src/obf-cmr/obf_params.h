#pragma once

#include "../circ_params.h"

struct obf_params_t {
    circ_params_t cp;
    int sigma;
    size_t npowers;
};
