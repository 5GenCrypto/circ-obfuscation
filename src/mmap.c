#include "mmap.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

void secret_params_init (secret_params *p, obf_params *op, level *toplevel, aes_randstate_t rng)
{
    mpz_t *moduli = lin_malloc((op->c+3) * sizeof(mpz_t));
    for (int i = 0; i < op->c+3; i++) {
        mpz_init(moduli[i]);
        mpz_urandomb_aes(moduli[i], rng, 128);
    }
    p->moduli = moduli;
    p->op = op;
    p->toplevel = toplevel;
}

void secret_params_clear (secret_params *p)
{
    level_destroy(p->toplevel);
    for (int i = 0; i < p->op->c+3; i++) {
        mpz_clear(p->moduli[i]);
    }
    free(p->moduli);
}

void public_params_init (public_params *p, secret_params *s)
{
    p->moduli = s->moduli;
    p->op = s->op;
    p->toplevel = s->toplevel;
}

void public_params_clear (public_params *p)
{
}

mpz_t* get_moduli (secret_params *s)
{
    return s->moduli;
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, public_params *p)
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

void encode (
    encoding *x,
    const mpz_t *inps,
    size_t nins,
    const level *lvl,
    secret_params *p,
    aes_randstate_t rng
){
    assert(nins == x->nslots);
    for (int i = 0; i < nins; i++) {
        mpz_set(x->slots[i], inps[i]);
    }
    level_set(x->lvl, lvl);
}

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    level_add(rop->lvl, x->lvl, y->lvl);
    for (int i = 0; i < rop->nslots; i++) {
        mpz_mul(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
}

void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(level_eq(x->lvl, y->lvl));
    for (int i = 0; i < rop->nslots; i++) {
        mpz_add(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
    level_set(rop->lvl, x->lvl);
}

void encoding_sub(encoding *rop, encoding *x, encoding *y, public_params *p)
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

int encoding_is_zero (encoding *x, public_params *p)
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
