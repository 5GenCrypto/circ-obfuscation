#ifndef __AB__OBFUSCATOR_H__
#define __AB__OBFUSCATOR_H__

#include "../obfuscator.h"

#define AB_FLAG_NONE   0x0
#define AB_FLAG_SIMPLE 0x1

extern obfuscator_vtable ab_obfuscator_vtable;
extern op_vtable ab_op_vtable;

#endif
