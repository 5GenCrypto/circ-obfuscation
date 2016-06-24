#include "fake_encoding.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// public parameters

void fake_params_init (fake_params *p, obf_params *op, mpz_t *moduli, level *toplevel)
{
    p->moduli = moduli;
    /*p->moduli = lin_malloc((op->c+3) * sizeof(mpz_t));*/
    /*for (int i = 0; i < op->c+3; i++) {*/
        /*mpz_init(p->moduli[i]);*/
        /*mpz_set(p->moduli[i], moduli[i]);*/
    /*}*/
    p->op = op;
    p->toplevel = toplevel;
}

void fake_params_clear (fake_params *p)
{
    level_destroy(p->toplevel);
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, fake_params *p)
{
    x->lvl = lin_malloc(sizeof(level));
    level_init(x->lvl, p->op);
    x->nslots = p->op->c+3;
    x->slots  = lin_malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }
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

void encoding_set (encoding *rop, encoding *x)
{
    rop->nslots = x->nslots;
    for (int i = 0; i < x->nslots; i++) {
        mpz_set(rop->slots[i], x->slots[i]);
    }
    level_set(rop->lvl, x->lvl);
}

void encode (encoding *x, const mpz_t *inps, size_t nins, const level *lvl)
{
    assert(nins == x->nslots);
    for (int i = 0; i < nins; i++) {
        mpz_set(x->slots[i], inps[i]);
    }
    level_set(x->lvl, lvl);
}

void encoding_mul (encoding *rop, encoding *x, encoding *y, fake_params *p)
{
    level_add(rop->lvl, x->lvl, y->lvl);
    for (int i = 0; i < rop->nslots; i++) {
        mpz_mul(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
}

void encoding_add (encoding *rop, encoding *x, encoding *y, fake_params *p)
{
    assert(level_eq(x->lvl, y->lvl));
    for (int i = 0; i < rop->nslots; i++) {
        mpz_add(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
    level_set(rop->lvl, x->lvl);
}

void encoding_sub(encoding *rop, encoding *x, encoding *y, fake_params *p)
{
    if (!level_eq(x->lvl, y->lvl)) {
        level_print(x->lvl);
        puts("");
        level_print(y->lvl);
        assert(level_eq(x->lvl, y->lvl));
    }
    for (int i = 0; i < rop->nslots; i++) {
        mpz_sub(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
    level_set(rop->lvl, x->lvl);
}

int encoding_eq (encoding *x, encoding *y)
{
    if (!level_eq(x->lvl, y->lvl))
        return 0;
    for (int i = 0; i < x->nslots; i++)
        if (x->slots[i] != y->slots[i])
            return 0;
    return 1;
}

int encoding_is_zero (encoding *x, fake_params *p)
{
    if(!level_eq(x->lvl, p->toplevel)) {
        level_print(x->lvl);
        level_print(p->toplevel);
        assert(level_eq(x->lvl, p->toplevel));
    }
    for (int i = 0; i < x->nslots; i++)
        if (mpz_sgn(x->slots[i]) != 0)
            return 0;
    return 1;
}
