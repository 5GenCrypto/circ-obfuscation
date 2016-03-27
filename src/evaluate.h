#ifndef __SRC_EVALUATE_H__
#define __SRC_EVALUATE_H__

#include "obfuscate.h"

typedef struct {
    encoding *r;
    encoding *z;
    size_t d;
} wire;

void evaluate (int *rop, const int *inps, obfuscation *obf, fake_params *p);

void wire_mul(wire *rop, wire *x, wire *y);
void wire_add(wire *rop, wire *x, wire *y, obfuscation *obf, fake_params *p);
void wire_sub(wire *rop, wire *x, wire *y, obfuscation *obf, fake_params *p);

#endif
