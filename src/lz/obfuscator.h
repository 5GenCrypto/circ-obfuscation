#ifndef __ZIM__OBFUSCATOR_H__
#define __ZIM__OBFUSCATOR_H__

#include "../obfuscator.h"

typedef struct lz_obf_params_t {
    size_t symlen;
} lz_obf_params_t;

extern obfuscator_vtable zim_obfuscator_vtable;
extern op_vtable zim_op_vtable;

#endif
