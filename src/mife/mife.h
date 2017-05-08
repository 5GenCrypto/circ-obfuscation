#pragma once

#include "mmap.h"

typedef struct mife_t mife_t;
typedef struct mife_sk_t mife_sk_t;
typedef struct mife_ek_t mife_ek_t;
typedef struct mife_ciphertext_t mife_ciphertext_t;

typedef struct {
    size_t npowers;
    size_t symlen;
    bool sigma;
} mife_params_t;

extern op_vtable mife_op_vtable;

mife_t *
mife_setup(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng);
void
mife_free(mife_t *mife);

mife_sk_t * mife_sk(const mife_t *mife);
void mife_sk_free(mife_sk_t *sk);
int mife_sk_fwrite(const mife_sk_t *sk, FILE *fp);
mife_sk_t * mife_sk_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp);

mife_ek_t * mife_ek(const mife_t *mife);
void mife_ek_free(mife_ek_t *ek);
int mife_ek_fwrite(const mife_ek_t *ek, FILE *fp);
mife_ek_t * mife_ek_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp);

void mife_ciphertext_free(mife_ciphertext_t *ct, const circ_params_t *cp);
int mife_ciphertext_fwrite(const mife_ciphertext_t *ct, const circ_params_t *cp, FILE *fp);
mife_ciphertext_t * mife_ciphertext_fread(const mmap_vtable *mmap, const circ_params_t *cp, FILE *fp);


mife_ciphertext_t *
mife_encrypt(const mife_sk_t *sk, size_t slot, const int *inputs,
             size_t npowers, size_t nthreads, aes_randstate_t rng);

int
mife_decrypt(const mife_ek_t *ek, int *rop, mife_ciphertext_t **cts,
             size_t nthreads, size_t *kappa);
