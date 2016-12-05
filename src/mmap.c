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
secret_params_init(const sp_vtable *const vt, secret_params *const sp,
                   const obf_params_t *const op, size_t lambda,
                   aes_randstate_t rng)
{
    return vt->init(vt->mmap, sp, op, lambda, rng);
}

void
secret_params_clear(const sp_vtable *const vt, secret_params *const sp)
{
    vt->clear(vt->mmap, sp);
}

void
public_params_init(const pp_vtable *const vt, const sp_vtable *const sp_vt,
                   public_params *const pp, const secret_params *const sp)
{
    vt->init(sp_vt, pp, sp);
    pp->pp = (mmap_pp *) vt->mmap->sk->pp(sp->sk);
}

int
public_params_fwrite(const pp_vtable *const vt, const public_params *const pp,
                     FILE *const fp)
{
    vt->fwrite(pp, fp);
    PUT_NEWLINE(fp);
    vt->mmap->pp->fwrite(pp->pp, fp);
    PUT_NEWLINE(fp);
    return 0;
}

int
public_params_fread(const pp_vtable *const vt, public_params *const pp,
                    const obf_params_t *const op, FILE *const fp)
{
    vt->fread(pp, op, fp);
    GET_NEWLINE(fp);
    pp->pp = malloc(vt->mmap->pp->size);
    vt->mmap->pp->fread(pp->pp, fp);
    GET_NEWLINE(fp);
    return 0;
}

void
public_params_clear(const pp_vtable *const vt, public_params *const pp)
{
    vt->clear(pp);
    vt->mmap->pp->clear(pp->pp);
}


////////////////////////////////////////////////////////////////////////////////
// encodings

encoding *
encoding_new(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
             const public_params *const pp)
{
    encoding *enc = calloc(1, sizeof(encoding));
    (void) vt->new(pp_vt, enc, pp);
    enc->enc = calloc(1, vt->mmap->enc->size);
    vt->mmap->enc->init(enc->enc, pp->pp);
    return enc;
}

void
encoding_free(const encoding_vtable *const vt, encoding *enc)
{
    vt->free(enc);
    vt->mmap->enc->clear(enc->enc);
    free(enc->enc);
    free(enc);
}

int
encoding_print(const encoding_vtable *const vt, const encoding *const enc)
{
    return vt->print(enc);
}

int
encode(const encoding_vtable *const vt, encoding *const rop,
       const mpz_t *const inps, size_t nins, const void *const set,
       const secret_params *const sp)
{
    fmpz_t finps[nins];
    int pows[vt->mmap->sk->nzs(sp->sk)];

    vt->encode(pows, rop, set);
    for (size_t i = 0; i < nins; ++i) {
        fmpz_init(finps[i]);
        fmpz_set_mpz(finps[i], inps[i]);
    }
    vt->mmap->enc->encode(rop->enc, sp->sk, nins, (fmpz_t *) finps, pows);
    for (size_t i = 0; i < nins; ++i) {
        fmpz_clear(finps[i]);
    }
    return 0;
}

int
encoding_set(const encoding_vtable *const vt, encoding *const rop,
             const encoding *const x)
{
    (void) vt->set(rop, x);
    vt->mmap->enc->set(rop->enc, x->enc);
    return 0;
}

int
encoding_mul(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
             encoding *const rop, const encoding *const x,
             const encoding *const y, const public_params *const p)
{
    (void) vt->mul(pp_vt, rop, x, y, p);
    vt->mmap->enc->mul(rop->enc, p->pp, x->enc, y->enc);
    return 0;
}

int
encoding_add(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
             encoding *const rop, const encoding *const x,
             const encoding *const y, const public_params *const p)
{
    (void) vt->add(pp_vt, rop, x, y, p);
    vt->mmap->enc->add(rop->enc, p->pp, x->enc, y->enc);
    return 0;
}

int
encoding_sub(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
             encoding *const rop, const encoding *const x,
             const encoding *const y, const public_params *const p)
{
    (void) vt->sub(pp_vt, rop, x, y, p);
    vt->mmap->enc->sub(rop->enc, p->pp, x->enc, y->enc);
    return 0;
}

int
encoding_is_zero(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
                 const encoding *const x, const public_params *const p)
{
    (void) vt->is_zero(pp_vt, x, p);
    return vt->mmap->enc->is_zero(x->enc, p->pp);
}

////////////////////////////////////////////////////////////////////////////////
// serialization

void
encoding_fread(const encoding_vtable *const vt, encoding *const x, FILE *const fp)
{
    vt->fread(x, fp);
    x->enc = calloc(1, vt->mmap->enc->size);
    vt->mmap->enc->fread(x->enc, fp);
}

void
encoding_fwrite(const encoding_vtable *const vt, const encoding *const x,
                FILE *const fp)
{
    vt->fwrite(x, fp);
    vt->mmap->enc->fwrite(x->enc, fp);
}
