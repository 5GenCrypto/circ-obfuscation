#ifndef __OBFUSCATOR_H__
#define __OBFUSCATOR_H__

#include "mmap.h"

typedef struct obfuscation obfuscation;
typedef struct {
    obfuscation * (*new)(const mmap_vtable *, const obf_params_t *, size_t, size_t);
    void (*free)(obfuscation *);
    int (*obfuscate)(obfuscation *);
    int (*evaluate)(int *, const int *, const obfuscation *);
    int (*fwrite)(const obfuscation *, FILE *);
    obfuscation * (*fread)(const mmap_vtable *, const obf_params_t *, FILE *);
} obfuscator_vtable;

#endif
