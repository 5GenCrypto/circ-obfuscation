#pragma once

#include <aesrand.h>
#include "mife.h"
#include "mmap.h"

int
mife_run_setup(const mmap_vtable *mmap, const mife_vtable *vt,
               const char *circuit, obf_params_t *op, size_t secparam,
               size_t *kappa, size_t npowers, size_t nthreads,
               aes_randstate_t rng);
int
mife_run_encrypt(const mmap_vtable *mmap, const mife_vtable *vt,
                 const char *circuit, obf_params_t *op, const long *input,
                 size_t slot, size_t nthreads, mife_sk_t *cached_sk,
                 aes_randstate_t rng);
int
mife_run_decrypt(const mmap_vtable *mmap, const mife_vtable *vt,
                 const char *ek_s, char **cts_s, long *rop,
                 obf_params_t *op, size_t *kappa, size_t nthreads);
int
mife_run_test(const mmap_vtable *mmap, const mife_vtable *vt,
              const char *circuit, obf_params_t *op, size_t secparam,
              size_t *kappa, size_t npowers, size_t nthreads,
              aes_randstate_t rng);
size_t
mife_run_smart_kappa(const mife_vtable *vt, const char *circuit,
                     const obf_params_t *op, size_t npowers, size_t nthreads,
                     aes_randstate_t rng);
