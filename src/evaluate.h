#ifndef __SRC_EVALUATE_H__
#define __SRC_EVALUATE_H__

#include "obfuscate.h"

typedef struct {
    encoding *r;
    encoding *z;
    int my_encodings; // whether this wire "owns" the encodings
    size_t d;
    size_t *type;
    size_t c; // size of type - 1
} wire;

void evaluate (int *rop, const int *inps, obfuscation *obf, fake_params *p);

void wire_init  (wire *rop, fake_params *p, int init_encodings);
void wire_clear (wire *rop);

void wire_mul (wire *rop, wire *x, wire *y);
void wire_add (wire *rop, wire *x, wire *y, obfuscation *obf, fake_params *p);
void wire_sub (wire *rop, wire *x, wire *y, obfuscation *obf, fake_params *p);

#endif
