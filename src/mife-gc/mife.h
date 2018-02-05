#pragma once

#include "../mife.h"

typedef struct {
    bool indexed;
    size_t padding;
    size_t wirelen;
} mife_gc_params_t;

extern mife_vtable mife_gc_vtable;
extern op_vtable mife_gc_op_vtable;
