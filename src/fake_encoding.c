#include "fake_encoding.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// public parameters

void fake_params_init (fake_params *p, obf_params *op, mpz_t *moduli)
{
    p->moduli = moduli;
    /*p->moduli = malloc((op->c+3) * sizeof(mpz_t));*/
    /*for (int i = 0; i < op->c+3; i++) {*/
        /*mpz_init(p->moduli[i]);*/
        /*mpz_set(p->moduli[i], moduli[i]);*/
    /*}*/
    p->op = op;
}

void fake_params_clear (fake_params *p)
{
    /*for (int i = 0; i < p->op->c+3; i++) {*/
        /*mpz_clear(p->moduli[i]);*/
    /*}*/
    /*free(p->moduli);*/
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, fake_params *p)
{
    x->lvl = malloc(sizeof(level));
    level_init(x->lvl, p->op);
    x->nslots = p->op->c+3;
    x->slots  = malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }
    x->d = 0;
}

void encoding_clear (encoding *x)
{
    level_clear(x->lvl);
    free(x->lvl);
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
    x->d = 1;
}

void encoding_mul (encoding *rop, encoding *x, encoding *y)
{
    level_add(rop->lvl, x->lvl, y->lvl);
    for (int i = 0; i < rop->nslots; i++) {
        mpz_mul(rop->slots[i], x->slots[i], y->slots[i]);
    }
    rop->d = x->d + y->d;
}

void encoding_add (encoding *rop, encoding *x, encoding *y)
{
    assert(x->d == y->d);
    assert(level_eq(x->lvl, y->lvl));
    for (int i = 0; i < rop->nslots; i++) {
        mpz_add(rop->slots[i], x->slots[i], y->slots[i]);
    }
    level_set(rop->lvl, x->lvl);
    rop->d = x->d;
}

void encoding_sub (encoding *rop, encoding *x, encoding *y)
{
    assert(x->d == y->d);
    assert(level_eq(x->lvl, y->lvl));
    for (int i = 0; i < rop->nslots; i++) {
        mpz_sub(rop->slots[i], x->slots[i], y->slots[i]);
    }
    level_set(rop->lvl, x->lvl);
    rop->d = x->d;
}
