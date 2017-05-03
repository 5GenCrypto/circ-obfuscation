#pragma once

#include "../obfuscator.h"

typedef struct mife_obf_params_t {
    size_t npowers;
    size_t symlen;
    bool sigma;
} mife_obf_params_t;

extern obfuscator_vtable mife_obfuscator_vtable;
extern op_vtable mife_op_vtable;

