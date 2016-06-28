#ifndef __LIN_FAKE_MMAP__
#define __LIN_FAKE_MMAP__

#include "level.h"
#include "obf_params.h"
#include "aesrand.h"
#include <clt13.h>
#include <stdlib.h>

typedef struct {
    mpz_t *moduli;
    obf_params *op;
    level *toplevel;

    clt_pp *pp;
} public_params;

typedef struct {
    mpz_t *moduli;
    obf_params *op;
    level *toplevel;

    clt_state *st;
} secret_params;

mpz_t* get_moduli (secret_params *s);

void secret_params_init (secret_params *p, obf_params *op, level *toplevel, size_t lambda, aes_randstate_t rng);
void secret_params_clear (secret_params *pp);

void public_params_init (public_params *p, secret_params *s);
void public_params_clear (public_params *pp);

typedef struct {
    level *lvl;
    size_t nslots;
    mpz_t *slots;   // of size c+3

    clt_elem_t clt;
} encoding;

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
