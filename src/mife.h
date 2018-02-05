#pragma once

#include <aesrand.h>
#include "mmap.h"

typedef struct mife_t mife_t;
typedef struct mife_sk_t mife_sk_t;
typedef struct mife_ek_t mife_ek_t;
typedef struct mife_ct_t mife_ct_t;

typedef struct {
    mife_t *    (*mife_setup)(const mmap_vtable *mmap, const obf_params_t *op,
                              size_t secparam, size_t *kappa, size_t nthreads,
                              aes_randstate_t rng);
    void        (*mife_free)(mife_t *mife);
    mife_sk_t * (*mife_sk)(const mife_t *mife);
    void        (*mife_sk_free)(mife_sk_t *sk);
    int         (*mife_sk_fwrite)(const mife_sk_t *sk, FILE *fp);
    mife_sk_t * (*mife_sk_fread)(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp);
    mife_ek_t * (*mife_ek)(const mife_t *mife);
    void        (*mife_ek_free)(mife_ek_t *ek);
    int         (*mife_ek_fwrite)(const mife_ek_t *ek, FILE *fp);
    mife_ek_t * (*mife_ek_fread)(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp);
    void        (*mife_ct_free)(mife_ct_t *ct);
    int         (*mife_ct_fwrite)(const mife_ct_t *ct, FILE *fp);
    mife_ct_t * (*mife_ct_fread)(const mmap_vtable *mmap, const mife_ek_t *ek, FILE *fp);
    mife_ct_t * (*mife_encrypt)(const mife_sk_t *sk, size_t slot, const long *inputs,
                                size_t ninputs, size_t nthreads, aes_randstate_t rng);
    int         (*mife_decrypt)(const mife_ek_t *ek, long *rop, const mife_ct_t **cts,
                                size_t nthreads, size_t *kappa);
} mife_vtable;
