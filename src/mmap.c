#include "mmap.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

void secret_params_init (secret_params *p, obf_params *op, level *toplevel, size_t lambda, aes_randstate_t rng)
{
    mpz_t *moduli = lin_malloc((op->c+3) * sizeof(mpz_t));
    for (int i = 0; i < op->c+3; i++) {
        mpz_init(moduli[i]);
        mpz_urandomb_aes(moduli[i], rng, 128);
    }
    p->moduli = moduli;
    p->op = op;
    p->toplevel = toplevel;

    p->st = lin_malloc(sizeof(clt_state));
    size_t kappa = 2;
    size_t nzs = (op->q+1) * (op->c+2) + op->gamma;
    int pows[nzs];
    level_flatten(pows, toplevel);
    clt_state_init(p->st, kappa, lambda, nzs, pows, CLT_FLAG_DEFAULT | CLT_FLAG_VERBOSE, rng);
}

void secret_params_clear (secret_params *p)
{
    level_destroy(p->toplevel);
    for (int i = 0; i < p->op->c+3; i++) {
        mpz_clear(p->moduli[i]);
    }
    free(p->moduli);

    clt_state_clear(p->st);
    free(p->st);
}

void public_params_init (public_params *p, secret_params *s)
{
    p->moduli = s->moduli;
    p->op = s->op;
    p->toplevel = s->toplevel;

    p->pp = lin_malloc(sizeof(clt_pp));
    clt_pp_init(p->pp, s->st);
}

void public_params_clear (public_params *p)
{
    clt_pp_clear(p->pp);
    free(p->pp);
}

mpz_t* get_moduli (secret_params *s)
{
    return s->moduli;
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, obf_params *p)
{
    x->lvl = lin_malloc(sizeof(level));
    level_init(x->lvl, p);
    x->nslots = p->c+3;
    x->slots  = lin_malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }

    mpz_init(x->clt);
}

void encoding_clear (encoding *x)
{
    level_clear(x->lvl);
    free(x->lvl);
    for (int i = 0; i < x->nslots; i++) {
        mpz_clear(x->slots[i]);
    }
    free(x->slots);

    mpz_clear(x->clt);
}

void encoding_set (encoding *rop, encoding *x)
{
    rop->nslots = x->nslots;
    for (int i = 0; i < x->nslots; i++) {
        mpz_set(rop->slots[i], x->slots[i]);
    }
    level_set(rop->lvl, x->lvl);

    mpz_set(rop->clt, x->clt);
}

void encode (
    encoding *x,
    mpz_t *inps,
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

    int pows[p->st->nzs];
    level_flatten(pows, lvl);
    clt_encode(x->clt, p->st, nins, inps, pows, rng);
}

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    level_add(rop->lvl, x->lvl, y->lvl);
    for (int i = 0; i < rop->nslots; i++) {
        mpz_mul(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }

    mpz_mul(rop->clt, x->clt, y->clt);
    mpz_mod(rop->clt, rop->clt, p->pp->x0);
}

void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(level_eq(x->lvl, y->lvl));
    for (int i = 0; i < rop->nslots; i++) {
        mpz_add(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
    level_set(rop->lvl, x->lvl);

    mpz_add(rop->clt, x->clt, y->clt);
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

    mpz_sub(rop->clt, x->clt, y->clt);
}

int encoding_eq (encoding *x, encoding *y)
{
    if (!level_eq(x->lvl, y->lvl))
        return 0;
    for (int i = 0; i < x->nslots; i++)
        if (mpz_cmp(x->slots[i], y->slots[i]) != 0)
            return 0;

    if (mpz_cmp(x->clt, y->clt) != 0)
        return 0;

    return 1;
}

int encoding_is_zero (encoding *x, public_params *p)
{
    bool ret = true;

    if(!level_eq(x->lvl, p->toplevel)) {
        level_print(x->lvl);
        level_print(p->toplevel);
        assert(level_eq(x->lvl, p->toplevel));
    }
    for (int i = 0; i < x->nslots; i++)
        ret &= mpz_sgn(x->slots[i]) == 0;

    return ret; // TODO: debug me when you know what kappa should be
    bool clt_res = clt_is_zero(p->pp, x->clt);
    assert(ret == clt_res);
    return clt_res;
}

////////////////////////////////////////////////////////////////////////////////
// serialization

void secret_params_read  (secret_params *x, FILE *const fp);
void secret_params_write (FILE *const fp, secret_params *x);
void public_params_read  (public_params *x, FILE *const fp);
void public_params_write (FILE *const fp, public_params *x);

void encoding_read (encoding *x, FILE *const fp)
{
    x->lvl = lin_malloc(sizeof(level));
    level_read(x->lvl, fp);
    GET_SPACE(fp);
    ulong_read(&x->nslots, fp);
    GET_SPACE(fp);
    x->slots = lin_malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }
    clt_vector_fread(fp, x->slots, x->nslots);
    GET_SPACE(fp);
    mpz_init(x->clt);
    clt_elem_fread(fp, x->clt);
}

void encoding_write (FILE *const fp, encoding *x)
{
    level_write(fp, x->lvl);
    PUT_SPACE(fp);
    ulong_write(fp, x->nslots);
    PUT_SPACE(fp);
    clt_vector_fsave(fp, x->slots, x->nslots);
    PUT_SPACE(fp);
    clt_elem_fsave(fp, x->clt);
}
