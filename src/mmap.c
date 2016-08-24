#include "mmap.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

void secret_params_init (
    secret_params *p,
    obf_params *op,
    level *toplevel,
    size_t lambda,
    aes_randstate_t rng
) {
    p->op = op;
    p->toplevel = toplevel;

#if FAKE_MMAP
    mpz_t *moduli = lin_malloc((op->c+3) * sizeof(mpz_t));
    for (int i = 0; i < op->c+3; i++) {
        mpz_init(moduli[i]);
        mpz_urandomb_aes(moduli[i], rng, 128);
    }
    p->moduli = moduli;
#endif
    size_t t = 0;
    for (int o = 0; o < op->gamma; o++) {
        size_t tmp = ARRAY_SUM(op->types[o], op->c+1);
        if (tmp > t)
            t = tmp;
    }
    size_t kappa = 2 + op->c + t + op->D;
    printf("kappa=%lu\n", kappa);
#if !FAKE_MMAP
    size_t nzs = (op->q+1) * (op->c+2) + op->gamma;
    int pows[nzs];
    level_flatten(pows, toplevel);
    p->clt_st = lin_malloc(sizeof(clt_state));
    clt_state_init(p->clt_st, kappa, lambda, nzs, pows, CLT_FLAG_DEFAULT | CLT_FLAG_VERBOSE, rng);
#endif
}

void secret_params_clear (secret_params *p)
{
    level_destroy(p->toplevel);
#if FAKE_MMAP
    for (int i = 0; i < p->op->c+3; i++) {
        mpz_clear(p->moduli[i]);
    }
    free(p->moduli);
#else
    clt_state_clear(p->clt_st);
    free(p->clt_st);
#endif
}

void public_params_init (public_params *p, secret_params *s)
{
    p->toplevel = s->toplevel;
    p->op = s->op;
#if FAKE_MMAP
    p->moduli = s->moduli;
#else
    p->clt_pp = lin_malloc(sizeof(clt_pp));
    clt_pp_init(p->clt_pp, s->clt_st);
#endif
}

void public_params_clear (public_params *p)
{
#if !FAKE_MMAP
    clt_pp_clear(p->clt_pp);
    free(p->clt_pp);
#endif
}

mpz_t* get_moduli (secret_params *s)
{
#if FAKE_MMAP
    return s->moduli;
#else
    return s->clt_st->gs;
#endif
}

////////////////////////////////////////////////////////////////////////////////
// encodings

void encoding_init (encoding *x, obf_params *p)
{
    x->lvl = lin_malloc(sizeof(level));
    level_init(x->lvl, p);
    x->nslots = p->c+3;
#if FAKE_MMAP
    x->slots  = lin_malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }
#else
    mpz_init(x->clt);
#endif
}

void encoding_clear (encoding *x)
{
    level_clear(x->lvl);
    free(x->lvl);
#if FAKE_MMAP
    for (int i = 0; i < x->nslots; i++) {
        mpz_clear(x->slots[i]);
    }
    free(x->slots);
#else
    mpz_clear(x->clt);
#endif
}

void encoding_set (encoding *rop, encoding *x)
{
    rop->nslots = x->nslots;
    level_set(rop->lvl, x->lvl);
#if FAKE_MMAP
    for (int i = 0; i < x->nslots; i++) {
        mpz_set(rop->slots[i], x->slots[i]);
    }
#else
    mpz_set(rop->clt, x->clt);
#endif
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
    level_set(x->lvl, lvl);
#if FAKE_MMAP
    for (int i = 0; i < nins; i++) {
        mpz_set(x->slots[i], inps[i]);
    }
#else
    int pows[p->clt_st->nzs];
    level_flatten(pows, lvl);
    clt_encode(x->clt, p->clt_st, nins, inps, pows, rng);
#endif
}

void encoding_mul (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    level_add(rop->lvl, x->lvl, y->lvl);
#if FAKE_MMAP
    for (int i = 0; i < rop->nslots; i++) {
        mpz_mul(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
#else
    mpz_mul(rop->clt, x->clt, y->clt);
    mpz_mod(rop->clt, rop->clt, p->clt_pp->x0);
#endif
}

void encoding_add (encoding *rop, encoding *x, encoding *y, public_params *p)
{
    assert(level_eq(x->lvl, y->lvl));
    level_set(rop->lvl, x->lvl);
#if FAKE_MMAP
    for (int i = 0; i < rop->nslots; i++) {
        mpz_add(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
#else
    mpz_add(rop->clt, x->clt, y->clt);
#endif
}

void encoding_sub(encoding *rop, encoding *x, encoding *y, public_params *p)
{
    if (!level_eq(x->lvl, y->lvl)) {
        printf("[encoding_sub] unequal levels!\nx=\n");
        level_print(x->lvl);
        printf("y=\n");
        level_print(y->lvl);
    }
    assert(level_eq(x->lvl, y->lvl));
    level_set(rop->lvl, x->lvl);
#if FAKE_MMAP
    for (int i = 0; i < rop->nslots; i++) {
        mpz_sub(rop->slots[i], x->slots[i], y->slots[i]);
        mpz_mod(rop->slots[i], rop->slots[i], p->moduli[i]);
    }
#else
    mpz_sub(rop->clt, x->clt, y->clt);
#endif
}

int encoding_eq (encoding *x, encoding *y)
{
    if (!level_eq(x->lvl, y->lvl))
        return 0;
#if FAKE_MMAP
    for (int i = 0; i < x->nslots; i++)
        if (mpz_cmp(x->slots[i], y->slots[i]) != 0)
            return 0;
#else
    if (mpz_cmp(x->clt, y->clt) != 0)
        return 0;
#endif
    return 1;
}

int encoding_is_zero (encoding *x, public_params *p)
{
    bool ret;
    if(!level_eq(x->lvl, p->toplevel)) {
        puts("this level:");
        level_print(x->lvl);
        puts("top level:");
        level_print(p->toplevel);
        assert(level_eq(x->lvl, p->toplevel));
    }
#if FAKE_MMAP
    ret = true;
    for (int i = 0; i < x->nslots; i++)
        ret &= mpz_sgn(x->slots[i]) == 0;
#else
    ret = clt_is_zero(p->clt_pp, x->clt);
#endif
    return ret;
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
#if FAKE_MMAP
    x->slots = lin_malloc(x->nslots * sizeof(mpz_t));
    for (int i = 0; i < x->nslots; i++) {
        mpz_init(x->slots[i]);
    }
    assert(clt_vector_fread(fp, x->slots, x->nslots) == 0);
#else
    mpz_init(x->clt);
    assert(clt_elem_fread(fp, x->clt) == 0);
#endif
}

void encoding_write (FILE *const fp, encoding *x)
{
    level_write(fp, x->lvl);
    PUT_SPACE(fp);
    ulong_write(fp, x->nslots);
    PUT_SPACE(fp);
#if FAKE_MMAP
    assert(clt_vector_fsave(fp, x->slots, x->nslots) == 0);
#else
    assert(clt_elem_fsave(fp, x->clt) == 0);
#endif
}
