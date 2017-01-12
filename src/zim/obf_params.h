#pragma once

#include <acirc.h>
#include <stddef.h>

struct obf_params_t {
    size_t ninputs;
    size_t nconsts;
    size_t noutputs;
    size_t npowers;
    acirc *circ;
};
