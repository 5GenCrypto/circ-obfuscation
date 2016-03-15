#ifndef __SRC_FAKE_ENCODING__
#define __SRC_FAKE_ENCODING__

#include "level.h"
#include "params.h"

typedef struct {
    mpz_t *moduli;
    size_t nslots;
} public_parameters;

void public_parameters_init  (public_parameters *pp, mpz_t *moduli, size_t nslots);
void public_parameters_clear (public_parameters *pp);

typedef struct {
    level *lvl;
    mpz_t *slots;   // of size c+3
    size_t nslots;
} encoding;

void encoding_init  (encoding *x, params *p);
void encoding_clear (encoding *x);

void encode (encoding *x, const mpz_t *inps, size_t nins, const level *lvl);

#endif
