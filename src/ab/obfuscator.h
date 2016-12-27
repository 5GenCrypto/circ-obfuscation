#ifndef __AB__OBFUSCATOR_H__
#define __AB__OBFUSCATOR_H__

#include "../obfuscator.h"

typedef struct ab_obf_params_t {
    bool simple;
} ab_obf_params_t;

extern obfuscator_vtable ab_obfuscator_vtable;
extern op_vtable ab_op_vtable;

#endif
