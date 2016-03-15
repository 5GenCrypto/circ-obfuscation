#include "fake_encoding.h"

#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// public parameters

void public_parameters_init (public_parameters *pp, mpz_t *moduli, size_t nslots)
{
    pp->moduli = malloc((p->c+3) * sizeof(mpz_t));
    for (int i = 0; i < p->c+3; i++) {
        mpz_init_set(pp->moduli[i], moduli[i]);
    }
    pp->nslots = nslots;
}

void public_parameters_clear (public_parameters *pp)
{
    for (int i = 0; i < p->c+3; i++) {
        mpz_clear(pp->moduli[i]);
    }
    free(pp->moduli);
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, parameters *p);
{
    x->p = p;
    x->nslots = p->c+3;
    level_init(x->lvl, p);
    x->slots = malloc((p->c+3) * sizeof(mpz_t));
    for (int i = 0; i < p->c+3; i++) {
        mpz_init(x->slots[i]);
    }
}

void encoding_clear (encoding *x)
{
    level_clear(x->lvl);
    for (int i = 0; i < x->nslots; i++) {
        mpz_clear(x->slots[i]);
    }
    free(x->slots);
}

void encode (encoding *x, const mpz_t *inps, size_t nins, const level *lvl)
{
    assert(nins == x->nslots);
    for (int i = 0; i < nins; i++) {
        mpz_set(x->slots[i], inps[i]);
    }
    level_set(x->lvl, lvl);
}
