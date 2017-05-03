#pragma once

#include "mmap.h"
#include "circ_params.h"
#include "mife.h"

#include <aesrand.h>

int
mife_run_setup(const mmap_vtable *mmap, const char *circuit, circ_params_t *cp,
               aes_randstate_t rng, size_t secparam, size_t *kappa,
               size_t nthreads);

int
mife_run_encrypt(const mmap_vtable *mmap, const char *circuit, circ_params_t *cp,
                 aes_randstate_t rng, const int *input, size_t slot, 
                 size_t *npowers, size_t nthreads, mife_sk_t *cached_sk);

int
mife_run_decrypt(const char *ek_s, char **cts_s, int *rop,
                 const mmap_vtable *mmap, circ_params_t *cp, size_t *kappa,
                 size_t nthreads);

int
mife_run_all(const mmap_vtable *mmap, const char *circuit,
             circ_params_t *cp, aes_randstate_t rng, const int **inp,
             int *outp, size_t *kappa, size_t *npowers, size_t nthreads);

int
mife_run_test(const mmap_vtable *mmap, const char *circuit, circ_params_t *cp,
              aes_randstate_t rng, size_t secparam, size_t *kappa,
              size_t *npowers, size_t nthreads);

size_t
mife_run_smart_kappa(const char *circuit, circ_params_t *cp, const acirc *circ,
                     size_t npowers, size_t nthreads, aes_randstate_t rng);
