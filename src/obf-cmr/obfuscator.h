#pragma once

#include "../obfuscator.h"

typedef struct {
    size_t npowers;
} obf_cmr_params_t;

size_t obf_cmr_num_encodings(const obf_params_t *op);

extern obfuscator_vtable obf_cmr_vtable;
extern op_vtable obf_cmr_op_vtable;
