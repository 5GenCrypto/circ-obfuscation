#ifndef __LIN_FAKE_MMAP__
#define __LIN_FAKE_MMAP__

#include "level.h"
#include "obf_params.h"
#include "aesrand.h"
#include <clt13.h>
#include <stdlib.h>

#define FAKE_MMAP 1

typedef struct {
    obf_params *op;
    level *toplevel;
#if FAKE_MMAP
    mpz_t *moduli;
#else
    clt_state *clt_st;
#endif
} secret_params;

typedef struct {
    obf_params *op;
    level *toplevel;
#if FAKE_MMAP
    mpz_t *moduli;
#else
    clt_pp *clt_pp;
#endif
} public_params;

typedef struct {
    level *lvl;
    size_t nslots;
#if FAKE_MMAP
    mpz_t *slots;   // of size c+3
#else
    clt_elem_t clt;
#endif
} encoding;

void secret_params_init (secret_params *p, obf_params *op, level *toplevel, size_t lambda, aes_randstate_t rng);
void secret_params_clear (secret_params *pp);
mpz_t* get_moduli (secret_params *s);

void public_params_init (public_params *p, secret_params *s);
void public_params_clear (public_params *pp);

void encoding_init  (encoding *x, obf_params *p);
void encoding_clear (encoding *x);
void encoding_set   (encoding *rop, encoding *x);

void encode (
    encoding *x,
    mpz_t *inps,
    size_t nins,
    const level *lvl,
    secret_params *p,
    aes_randstate_t rng
);

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_sub (encoding *rop, encoding *x, encoding *y, public_params *p);
int  encoding_eq  (encoding *x, encoding *y);

int encoding_is_zero (encoding *x, public_params *p);

void secret_params_read  (secret_params *x, FILE *const fp);
void secret_params_write (FILE *const fp, secret_params *x);
void public_params_read  (public_params *x, FILE *const fp);
void public_params_write (FILE *const fp, public_params *x);
void encoding_read  (encoding *x, FILE *const fp);
void encoding_write (FILE *const fp, encoding *x);

#endif
