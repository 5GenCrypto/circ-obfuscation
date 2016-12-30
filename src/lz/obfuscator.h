#ifndef __LZ__OBFUSCATOR_H__
#define __LZ__OBFUSCATOR_H__

#include "../obfuscator.h"

typedef struct lz_obf_params_t {
    size_t npowers;
    size_t symlen;
    size_t rachel_inputs;
} lz_obf_params_t;

extern obfuscator_vtable lz_obfuscator_vtable;
extern op_vtable lz_op_vtable;

#endif
