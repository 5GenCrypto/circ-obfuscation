#pragma once

#include "../obfuscator.h"

typedef struct lin_obf_params_t {
    size_t symlen;
    bool sigma;
} lin_obf_params_t;

extern obfuscator_vtable lin_obfuscator_vtable;
extern op_vtable lin_op_vtable;
