#ifndef __SRC_FAKE_ENCODING__
#define __SRC_FAKE_ENCODING__

#include "level.h"
#include "obf_params.h"

typedef struct {
    mpz_t *moduli;
    obf_params *op;
} fake_params;

void fake_params_init (fake_params *p, obf_params *op, mpz_t *moduli);
void fake_params_clear (fake_params *pp);

typedef struct {
    level *lvl;
    mpz_t *slots;   // of size c+3
    size_t nslots;
} encoding;

void encoding_init  (encoding *x, fake_params *p);
void encoding_clear (encoding *x);

void encode (encoding *x, const mpz_t *inps, size_t nins, const level *lvl);

#endif
