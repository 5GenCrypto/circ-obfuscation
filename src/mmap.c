#include "mmap.h"

#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <clt13.h>

////////////////////////////////////////////////////////////////////////////////
// parameters

void
secret_params_init(const mmap_vtable *mmap, secret_params *p, obf_params *op,
                   size_t lambda, aes_randstate_t rng)
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
    kappa = 2 + op->n + t + op->D;
    printf("kappa=%lu\n", kappa);
    nzs = (op->n + op->m + 1) * (op->simple ? 3 : 4);
    printf("nzs=%lu\n", nzs);

    {
        int pows[nzs];
        level_flatten(pows, p->toplevel);
        p->sk = lin_malloc(mmap->sk->size);
        mmap->sk->init(p->sk, lambda, kappa, op->nslots, nzs, pows, 1, rng, true);
    }
}

void
secret_params_clear(const mmap_vtable *mmap, secret_params *p)
{
    level_free(p->toplevel);
    mmap->sk->clear(p->sk);
    free(p->sk);
}

void
public_params_init(const mmap_vtable *mmap, public_params *p, secret_params *s)
{
    p->toplevel = s->toplevel;
    p->op = s->op;
    p->pp = (mmap_pp *) mmap->sk->pp(s->sk);
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
    level_print(enc->lvl);
    mmap->enc->print(&enc->enc);
}

void
encoding_set(const mmap_vtable *mmap, encoding *rop, encoding *x)
{
    level_set(rop->lvl, x->lvl);
    mmap->enc->set(&rop->enc, &x->enc);
}

void
encode(const mmap_vtable *mmap, encoding *x, mpz_t *inps, size_t nins,
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
    mmap->enc->encode(&x->enc, sp->sk, nins, finps, pows);
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

void secret_params_read  (secret_params *x, FILE *const fp);
void secret_params_write (FILE *const fp, secret_params *x);
void public_params_read  (public_params *x, FILE *const fp);
void public_params_write (FILE *const fp, public_params *x);

void
encoding_read(const mmap_vtable *mmap, encoding *x, FILE *const fp)
{
    x->lvl = lin_malloc(sizeof(level));
    level_read(x->lvl, fp);
    GET_SPACE(fp);
    mmap->enc->fread(&x->enc, fp);
}

void
encoding_write(const mmap_vtable *mmap, encoding *x, FILE *const fp)
{
    level_write(fp, x->lvl);
    (void) PUT_SPACE(fp);
    mmap->enc->fwrite(&x->enc, fp);
}
