#ifndef __SRC_EVALUATE_H__
#define __SRC_EVALUATE_H__

#include "obfuscate.h"
#include <stdbool.h>

typedef struct {
    encoding *r;
    encoding *z;
} wire;

void evaluate (bool *rop, const bool *inps, obfuscation *obf, fake_params *p);

#endif
