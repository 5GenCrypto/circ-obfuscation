#pragma once

#include "obfuscator.h"

int
obf_run_obfuscate(const mmap_vtable *mmap, const obfuscator_vtable *vt,
                  const char *fname, const obf_params_t *op, size_t secparam,
                  size_t *kappa, size_t nthreads, aes_randstate_t rng);

int
obf_run_evaluate(const mmap_vtable *mmap, const obfuscator_vtable *vt,
                 const char *fname, const acirc_t *circ, const long *input,
                 size_t ninputs, long *output, size_t noutputs, size_t nthreads,
                 size_t *kappa, size_t *npowers);

size_t
obf_run_smart_kappa(const obfuscator_vtable *vt, const acirc_t *circ,
                    const obf_params_t *op, size_t nthreads, aes_randstate_t rng);
