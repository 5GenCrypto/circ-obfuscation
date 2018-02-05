#pragma once

#include "../mife.h"
#include "../circ_params.h"

struct obf_params_t {
    const acirc_t *circ;
    const mife_vtable *vt;
    bool indexed;
    size_t padding;
};
