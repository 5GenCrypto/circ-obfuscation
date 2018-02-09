#pragma once

#include "../mife.h"
#include "../circ_params.h"

struct obf_params_t {
    const acirc_t *circ;
    const mife_vtable *vt;
    size_t npowers;
    size_t padding;
    size_t wirelen;
    size_t ngates;
};
