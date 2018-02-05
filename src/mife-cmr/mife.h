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
typedef mife_ek_t mife_cmr_mife_ek_t;
typedef mife_ct_t mife_cmr_mife_ct_t;

mife_ct_t *
mife_cmr_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
                 size_t ninputs, size_t nthreads, aes_randstate_t rng,
                 mife_encrypt_cache_t *cache, mpz_t *_alphas);

int
mife_cmr_decrypt(const mife_ek_t *ek, long *rop, const mife_ct_t **cts,
                 size_t nthreads, size_t *kappa, size_t save);


extern mife_vtable mife_cmr_vtable;
extern op_vtable mife_cmr_op_vtable;

size_t mife_num_encodings_setup(const acirc_t *circ, size_t npowers);
size_t mife_num_encodings_encrypt(const acirc_t *circ, size_t slot);
