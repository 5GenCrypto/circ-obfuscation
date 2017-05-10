#pragma once

#include "mmap.h"

typedef struct obfuscation obfuscation;
typedef struct {
    obfuscation * (*obfuscate)(const mmap_vtable *mmap, const obf_params_t *op,
                               size_t secparam, size_t *kappa, size_t nthreads,
                               aes_randstate_t rng);
    void (*free)(obfuscation *obf);
    int (*evaluate)(const obfuscation *obf, int *outputs, size_t noutputs,
                    const int *inputs, size_t ninputs, size_t nthreads,
                    size_t *kappa, size_t *npowers);
    int (*fwrite)(const obfuscation *obf, FILE *fp);
    obfuscation * (*fread)(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp);
} obfuscator_vtable;
