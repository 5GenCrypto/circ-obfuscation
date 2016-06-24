#ifndef __SRC_FAKE_ENCODING__
#define __SRC_FAKE_ENCODING__

#include "level.h"
#include "obf_params.h"

typedef struct {
    mpz_t *moduli;
    obf_params *op;
    level *toplevel;
} fake_params;

void fake_params_init (fake_params *p, obf_params *op, mpz_t *moduli, level *toplevel);
void fake_params_clear (fake_params *pp);

typedef struct {
    level *lvl;
    mpz_t *slots;   // of size c+3
    size_t nslots;
} encoding;

void encoding_init  (encoding *x, fake_params *p);
void encoding_clear (encoding *x);
void encoding_set   (encoding *rop, encoding *x);

void encode (encoding *x, const mpz_t *inps, size_t nins, const level *lvl);

void encoding_mul (encoding *rop, encoding *x, encoding *y);
void encoding_add (encoding *rop, encoding *x, encoding *y);
void encoding_sub (encoding *rop, encoding *x, encoding *y);
int  encoding_eq  (encoding *x, encoding *y);

int encoding_is_zero (encoding *x, fake_params *p);

#endif
