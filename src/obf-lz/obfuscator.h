#pragma once

#include "../obfuscator.h"

typedef struct {
    size_t npowers;
    bool sigma;
} obf_lz_params_t;

extern obfuscator_vtable obf_lz_vtable;
extern op_vtable obf_lz_op_vtable;
