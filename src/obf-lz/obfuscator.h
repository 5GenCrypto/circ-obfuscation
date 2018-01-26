#pragma once

#include "../obfuscator.h"

typedef struct {
    size_t npowers;
    bool sigma;
} lz_obf_params_t;

extern obfuscator_vtable lz_obfuscator_vtable;
extern op_vtable lz_op_vtable;
