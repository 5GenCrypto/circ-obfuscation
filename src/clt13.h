#ifndef __CLT13_H__
#define __CLT13_H__

#include <gmp.h>

typedef unsigned long ulong;

typedef struct {
    gmp_randstate_t rng;
    ulong n;
    ulong nzs;
    ulong rho;
    ulong nu;
    mpz_t x0;
    mpz_t pzt;
    mpz_t *gs;
    mpz_t *crt_coeffs;
    mpz_t *zinvs;
} clt_state;

typedef struct {
    mpz_t x0;
    mpz_t pzt;
    ulong nu;
} clt_public_parameters;

int clt_setup(
    clt_state *s,
    ulong kappa,
    ulong lambda,
    ulong nzs,
    const int *pows,
    const char *dir,
    int verbose
);

void clt_pp_init(
    clt_state *mmap,
    clt_public_parameters *pp
);

void clt_state_clear(clt_state *s);

void clt_pp_clear(clt_public_parameters *pp);

void clt_encode(
    clt_state *s,
    mpz_t out,
    size_t nins,
    const mpz_t *ins,
    const int *pows
);

int clt_is_zero(
    clt_public_parameters *pp,
    const mpz_t c
);

#endif
