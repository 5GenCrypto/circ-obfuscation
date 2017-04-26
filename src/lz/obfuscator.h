#pragma once

#include "../obfuscator.h"

typedef struct lz_obf_params_t {
    size_t npowers;
    size_t symlen;
    bool sigma;
} lz_obf_params_t;

extern obfuscator_vtable lz_obfuscator_vtable;
extern op_vtable lz_op_vtable;
