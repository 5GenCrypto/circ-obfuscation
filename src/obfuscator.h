#pragma once

#include "./mmap.h"

typedef struct obfuscation obfuscation;
typedef struct {
    obfuscation * (*new)(const mmap_vtable *mmap, const obf_params_t *op,
                         const secret_params *sp, size_t secparam, size_t *kappa,
                         size_t ncores);
    void (*free)(obfuscation *obf);
    int (*obfuscate)(obfuscation *obf, size_t nthreads);
    int (*evaluate)(const obfuscation *obf, int *rop, const int *inputs,
                    size_t nthreads, size_t *degree, size_t *max_npowers);
    int (*fwrite)(const obfuscation *obf, FILE *fp);
    obfuscation * (*fread)(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp);
} obfuscator_vtable;
