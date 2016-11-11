#ifndef __AB__OBFUSCATE_H__
#define __AB__OBFUSCATE_H__

#include "mmap.h"

typedef struct obfuscation obfuscation;

obfuscation *
obfuscation_new(const mmap_vtable *mmap, const obf_params_t *const params,
                size_t secparam);
void
obfuscation_free(obfuscation *obf);

void
obfuscate(obfuscation *obf);

void
obfuscation_fwrite(const obfuscation *const obf, FILE *const fp);
obfuscation *
obfuscation_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *const fp);

void
obfuscation_eval(const mmap_vtable *mmap, int *rop, const int *inps,
                 const obfuscation *const obf);

#endif
