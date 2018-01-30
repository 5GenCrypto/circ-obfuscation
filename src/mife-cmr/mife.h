#pragma once

#include "../mife.h"
#include "../mmap.h"
#include <threadpool.h>

typedef struct {
    size_t npowers;
} mife_cmr_params_t;

typedef struct {
    threadpool *pool;
    pthread_mutex_t *lock;
    size_t *count;
    size_t total;
} mife_encrypt_cache_t;

typedef mife_t mife_cmr_mife_t;
typedef mife_sk_t mife_cmr_mife_sk_t;

mife_ct_t *
_mife_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
              size_t nthreads, aes_randstate_t rng, mife_encrypt_cache_t *cache,
              mpz_t *alphas, bool parallelize_circ_eval);

extern mife_vtable mife_cmr_vtable;
extern op_vtable mife_cmr_op_vtable;

size_t mife_num_encodings_setup(const circ_params_t *cp, size_t npowers);
size_t mife_num_encodings_encrypt(const circ_params_t *cp, size_t slot);
