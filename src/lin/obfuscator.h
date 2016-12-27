#ifndef __LIN__OBFUSCATOR_H__
#define __LIN__OBFUSCATOR_H__

#include "../obfuscator.h"

typedef struct lin_obf_params_t {
    bool rachel_input;
    size_t num_symbolic_inputs;
} lin_obf_params_t;

extern obfuscator_vtable lin_obfuscator_vtable;
extern op_vtable lin_op_vtable;

#endif
