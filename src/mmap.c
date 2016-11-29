#include "mmap.h"
#include "dbg.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <clt13.h>

extern unsigned int g_verbose;

static inline size_t ARRAY_SUM(size_t *xs, size_t n)
{
    size_t res = 0;
    for (size_t i = 0; i < n; ++i) {
        res += xs[i];
    }
    return res;
}

int
secret_params_init(const mmap_vtable *mmap, secret_params *p,
                   const obf_params_t *const op, size_t lambda,
                   aes_randstate_t rng)
{
    size_t t, kappa, nzs;

    p->op = op;
    p->toplevel = level_create_vzt(op);

    t = 0;
    for (size_t o = 0; o < op->gamma; o++) {
        size_t tmp = ARRAY_SUM(op->types[o], op->n + 1);
        if (tmp > t)
            t = tmp;
    }
    kappa = acirc_max_total_degree(op->circ) + op->n + 1;
    log_info("kappa=%lu", kappa);
    nzs = (op->n + op->m + 1) * (op->simple ? 3 : 4);
    log_info("nzs=%lu", nzs);

    {
        int pows[nzs];
        int res;

        level_flatten(pows, p->toplevel);
        p->sk = lin_malloc(mmap->sk->size);
        res = mmap->sk->init(p->sk, lambda, kappa, nzs, pows, op->nslots, 1,
                             rng, g_verbose);
        if (res) {
            log_err("mmap generation failed\n");
            free(p->sk);
            p->sk = NULL;
            return 1;
        }
    }
    return 0;
}

void
secret_params_clear(const mmap_vtable *mmap, secret_params *sp)
{
    level_free(sp->toplevel);
    if (sp->sk) {
        mmap->sk->clear(sp->sk);
        free(sp->sk);
    }
}

void
public_params_init(const mmap_vtable *mmap, public_params *pp, secret_params *sp)
{
    pp->toplevel = sp->toplevel;
    pp->op = sp->op;
    pp->pp = (mmap_pp *) mmap->sk->pp(sp->sk);
}

int
public_params_fwrite(const mmap_vtable *mmap, const public_params *const pp,
                     FILE *const fp)
{
    level_fwrite(pp->toplevel, fp);
    PUT_NEWLINE(fp);
    mmap->pp->fwrite(pp->pp, fp);
    PUT_NEWLINE(fp);
    return 0;
}

int
public_params_fread(const mmap_vtable *const mmap, public_params *pp,
                    const obf_params_t *op, FILE *const fp)
{
    pp->toplevel = level_create_vzt(op);
    level_fread(pp->toplevel, fp);
    GET_NEWLINE(fp);
    pp->op = op;
    pp->pp = malloc(mmap->pp->size);
    mmap->pp->fread(pp->pp, fp);
    GET_NEWLINE(fp);
    return 0;
}

void
public_params_clear(const mmap_vtable *mmap, public_params *p)
{
    mmap->pp->clear(p->pp);
}


////////////////////////////////////////////////////////////////////////////////
// encodings

encoding *
encoding_new(const mmap_vtable *mmap, public_params *pp)
{
    encoding *x = lin_malloc(sizeof(encoding));
    x->lvl = level_new(pp->op);
    mmap->enc->init(&x->enc, pp->pp);
    return x;
}

void
encoding_free(const mmap_vtable *mmap, encoding *x)
{
    if (x->lvl)
        level_free(x->lvl);
    mmap->enc->clear(&x->enc);
    free(x);
}

void
encoding_print(const mmap_vtable *const mmap, encoding *enc)
{
    (void) mmap;
    level_print(enc->lvl);
}

void
encoding_set(const mmap_vtable *mmap, encoding *rop, encoding *x)
{
    level_set(rop->lvl, x->lvl);
    mmap->enc->set(&rop->enc, &x->enc);
}

void
encode(const mmap_vtable *const mmap, encoding *x, mpz_t *inps, size_t nins,
       const level *lvl, secret_params *sp)
{
    fmpz_t finps[nins];
    int pows[mmap->sk->nzs(sp->sk)];

    level_set(x->lvl, lvl);
    level_flatten(pows, lvl);
    for (size_t i = 0; i < nins; ++i) {
        fmpz_init(finps[i]);
        fmpz_set_mpz(finps[i], inps[i]);
    }
    mmap->enc->encode(&x->enc, sp->sk, nins, (fmpz_t *) finps, pows);
    for (size_t i = 0; i < nins; ++i) {
        fmpz_clear(finps[i]);
    }
}

int
encoding_mul(const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y,
             public_params *p)
{
    level_add(rop->lvl, x->lvl, y->lvl);
    mmap->enc->mul(&rop->enc, p->pp, &x->enc, &y->enc);
    return 0;
}

int
encoding_add(const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y,
             public_params *p)
{
    assert(level_eq(x->lvl, y->lvl));
    level_set(rop->lvl, x->lvl);
    mmap->enc->add(&rop->enc, p->pp, &x->enc, &y->enc);
    return 0;
}

int
encoding_sub(const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y,
             public_params *p)
{
    if (!level_eq(x->lvl, y->lvl)) {
        printf("[encoding_sub] unequal levels!\nx=\n");
        level_print(x->lvl);
        printf("y=\n");
        level_print(y->lvl);
    }
    assert(level_eq(x->lvl, y->lvl));
    level_set(rop->lvl, x->lvl);
    mmap->enc->sub(&rop->enc, p->pp, &x->enc, &y->enc);
    return 0;
}

int
encoding_eq(encoding *x, encoding *y)
{
    if (!level_eq(x->lvl, y->lvl))
        return 0;
    /* TODO: add equality check to libmmap */
    return 1;
}

int
encoding_is_zero(const mmap_vtable *mmap, encoding *x, public_params *p)
{
    if (!level_eq(x->lvl, p->toplevel)) {
        puts("this level:");
        level_print(x->lvl);
        puts("top level:");
        level_print(p->toplevel);
    }
    assert(level_eq(x->lvl, p->toplevel));
    return mmap->enc->is_zero(&x->enc, p->pp);
}

////////////////////////////////////////////////////////////////////////////////
// serialization

void
encoding_fread(const mmap_vtable *const mmap, encoding *x, FILE *const fp)
{
    x->lvl = lin_calloc(1, sizeof(level));
    level_fread(x->lvl, fp);
    GET_NEWLINE(fp);
    mmap->enc->fread(&x->enc, fp);
}

void
encoding_fwrite(const mmap_vtable *const mmap, const encoding *const x, FILE *const fp)
{
    level_fwrite(x->lvl, fp);
    PUT_NEWLINE(fp);
    mmap->enc->fwrite(&x->enc, fp);
}
