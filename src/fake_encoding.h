#ifndef __SRC_FAKE_ENCODING__
#define __SRC_FAKE_ENCODING__

#include "level.h"
#include "obf_params.h"

typedef struct {
    mpz_t *moduli;
    obf_params *op;
    level *toplevel;
} public_params;

void public_params_init (public_params *p, obf_params *op, mpz_t *moduli, level *toplevel);
void public_params_clear (public_params *pp);

typedef struct {
    level *lvl;
    mpz_t *slots;   // of size c+3
    size_t nslots;
} encoding;

void encoding_init  (encoding *x, public_params *p);
void encoding_clear (encoding *x);
void encoding_set   (encoding *rop, encoding *x);

void encode (encoding *x, const mpz_t *inps, size_t nins, const level *lvl);

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p);
void encoding_sub (encoding *rop, encoding *x, encoding *y, public_params *p);
int  encoding_eq  (encoding *x, encoding *y);

int encoding_is_zero (encoding *x, public_params *p);

#endif
