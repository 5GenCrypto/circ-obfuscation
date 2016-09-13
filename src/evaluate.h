#ifndef __SRC_EVALUATE_H__
#define __SRC_EVALUATE_H__

#include "obfuscate.h"

void
evaluate(const mmap_vtable *mmap, int *rop, const int *inps, obfuscation *obf,
         public_params *p);

#endif
