#ifndef __OBFUSCATOR_H__
#define __OBFUSCATOR_H__

#include "mmap.h"

typedef struct obfuscation obfuscation;
typedef struct {
    obfuscation * (*new)(const mmap_vtable *const, const obf_params_t *const, size_t);
    void (*free)(obfuscation *);
    void (*obfuscate)(obfuscation *const);
    void (*evaluate)(int *rop, const int *inps, const obfuscation *const);
    void (*fwrite)(const obfuscation *, FILE *const);
    obfuscation * (*fread)(const mmap_vtable *const, const obf_params_t *const, FILE *const);
} obfuscator_vtable;

#endif
