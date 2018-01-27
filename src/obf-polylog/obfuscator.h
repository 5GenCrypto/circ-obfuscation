#pragma once

#include "../obfuscator.h"

typedef struct {
    size_t wordsize;
} polylog_obf_params_t;

extern obfuscator_vtable polylog_obfuscator_vtable;
extern op_vtable polylog_op_vtable;
