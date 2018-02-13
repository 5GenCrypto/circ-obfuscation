#pragma once

#include "../mife.h"

struct obf_params_t {
    const mife_vtable *vt;
    size_t npowers;
    size_t padding;
    size_t wirelen;
    size_t ngates;
};
