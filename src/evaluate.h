#ifndef __SRC_EVALUATE_H__
#define __SRC_EVALUATE_H__

#include "obfuscate.h"

typedef struct {
    encoding *r;
    encoding *z;
    int my_r; // whether this wire "owns" r
    int my_z; // whether this wire "owns" z
    size_t d;
} wire;

void evaluate (int *rop, const int *inps, obfuscation *obf, public_params *p);

void wire_init  (wire *rop, public_params *p, int init_r, int init_z);
void wire_clear (wire *rop);
void wire_copy  (wire *rop, wire *source, public_params *p);
void wire_init_from_encodings (wire *rop, public_params *p, encoding *r, encoding *z);

void wire_mul (wire *rop, wire *x, wire *y, public_params *p);
void wire_add (wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p);
void wire_sub (wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p);
void wire_constrained_sub (wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p);
void wire_constrained_add (wire *rop, wire *x, wire *y, obfuscation *obf, public_params *p);

int wire_type_eq (wire *x, wire *y);

#endif
