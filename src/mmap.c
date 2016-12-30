#include "mmap.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <clt13.h>

void
mmap_params_fprint(FILE *fp, const mmap_params_t *params)
{
    fprintf(fp, "mmap parameter settings:\n");
    fprintf(fp, "* κ:       %lu\n", params->kappa);
    fprintf(fp, "* # Zs:    %lu\n", params->nzs);
    fprintf(fp, "* # slots: %lu\n", params->nslots);
    fprintf(fp, "* toplevel: ");
    for (size_t i = 0; i < params->nzs; ++i) {
        fprintf(fp, "%d ", params->pows[i]);
    }
    fprintf(fp, "\n");
}

////////////////////////////////////////////////////////////////////////////////
// secret params

secret_params *
secret_params_new(const sp_vtable *vt, const obf_params_t *op, size_t lambda,
                  size_t kappa, aes_randstate_t rng)
{
    secret_params *sp = my_calloc(1, sizeof(secret_params));
    const mmap_params_t params = vt->init(sp, op, kappa);
    mmap_params_fprint(stderr, &params);
    sp->sk = calloc(1, vt->mmap->sk->size);
    if (vt->mmap->sk->init(sp->sk, lambda, params.kappa, params.nzs,
                           params.pows, params.nslots, 1, rng, g_verbose)) {
        free(sp);
        sp = NULL;
    }
    if (params.my_pows)
        free(params.pows);
    return sp;
}

void
secret_params_free(const sp_vtable *vt, secret_params *sp)
{
    vt->clear(sp);
    if (sp->sk) {
        vt->mmap->sk->clear(sp->sk);
        free(sp->sk);
    }
    free(sp);
}

////////////////////////////////////////////////////////////////////////////////
// public params

public_params *
public_params_new(const pp_vtable *vt, const sp_vtable *sp_vt,
                  const secret_params *sp)
{
    public_params *pp = my_calloc(1, sizeof(public_params));
    vt->init(sp_vt, pp, sp);
    pp->pp = vt->mmap->sk->pp(sp->sk);
    pp->my_pp = false;
    return pp;
}

int
public_params_fwrite(const pp_vtable *vt, const public_params *pp, FILE *fp)
{
    vt->fwrite(pp, fp);
    PUT_NEWLINE(fp);
    vt->mmap->pp->fwrite(pp->pp, fp);
    PUT_NEWLINE(fp);
    return OK;
}

public_params *
public_params_fread(const pp_vtable *vt, const obf_params_t *op, FILE *fp)
{
    public_params *pp = my_calloc(1, sizeof(public_params));
    vt->fread(pp, op, fp);
    GET_NEWLINE(fp);
    pp->pp = my_calloc(1, vt->mmap->pp->size);
    vt->mmap->pp->fread(pp->pp, fp);
    pp->my_pp = true;
    GET_NEWLINE(fp);
    return pp;
}

void
public_params_free(const pp_vtable *vt, public_params *pp)
{
    vt->clear(pp);
    vt->mmap->pp->clear(pp->pp);
    if (pp->my_pp)
        free(pp->pp);
    free(pp);
}

////////////////////////////////////////////////////////////////////////////////
// encodings

encoding *
encoding_new(const encoding_vtable *vt, const pp_vtable *pp_vt,
             const public_params *pp)
{
    encoding *enc = calloc(1, sizeof(encoding));
    (void) vt->new(pp_vt, enc, pp);
    enc->enc = calloc(1, vt->mmap->enc->size);
    vt->mmap->enc->init(enc->enc, pp->pp);
    return enc;
}

void
encoding_free(const encoding_vtable *vt, encoding *enc)
{
    vt->mmap->enc->clear(enc->enc);
    free(enc->enc);
    vt->free(enc);
    free(enc);
}

int
encoding_print(const encoding_vtable *vt, const encoding *enc)
{
    (void) vt->print(enc);
    vt->mmap->enc->print(enc->enc);
    return OK;
}

int
encode(const encoding_vtable *vt, encoding *rop, mpz_t *inps, size_t nins,
       const void *set, const secret_params *sp)
{
    fmpz_t finps[nins];
    int *pows;

    pows = vt->encode(rop, set);
    for (size_t i = 0; i < nins; ++i) {
        fmpz_init(finps[i]);
        fmpz_set_mpz(finps[i], inps[i]);
    }
    vt->mmap->enc->encode(rop->enc, sp->sk, nins, (const fmpz_t *) finps, pows);
    for (size_t i = 0; i < nins; ++i) {
        fmpz_clear(finps[i]);
    }
    free(pows);
    return OK;
}

int
encoding_set(const encoding_vtable *vt, encoding *rop, const encoding *x)
{
    (void) vt->set(rop, x);
    vt->mmap->enc->set(rop->enc, x->enc);
    return OK;
}

int
encoding_mul(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
             const encoding *x, const encoding *y, const public_params *p)
{

    if (vt->mul(pp_vt, rop, x, y, p) == ERR)
        return ERR;
    vt->mmap->enc->mul(rop->enc, p->pp, x->enc, y->enc);
    if (LOG_INFO) {
        printf("[%s]\n", __func__);
        encoding_print(vt, x);
        encoding_print(vt, y);
        encoding_print(vt, rop);
    }
    return OK;
}

int
encoding_add(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
             const encoding *x, const encoding *y, const public_params *p)
{
    if (vt->add(pp_vt, rop, x, y, p) == ERR)
        return ERR;
    vt->mmap->enc->add(rop->enc, p->pp, x->enc, y->enc);
    if (LOG_INFO) {
        printf("[%s]\n", __func__);
        encoding_print(vt, x);
        encoding_print(vt, y);
        encoding_print(vt, rop);
    }
    return OK;
}

int
encoding_sub(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
             const encoding *x, const encoding *y, const public_params *p)
{
    if (vt->sub(pp_vt, rop, x, y, p) == ERR)
        return ERR;
    vt->mmap->enc->sub(rop->enc, p->pp, x->enc, y->enc);
    if (LOG_INFO) {
        printf("[%s]\n", __func__);
        encoding_print(vt, x);
        encoding_print(vt, y);
        encoding_print(vt, rop);
    }
    return OK;
}

int
encoding_is_zero(const encoding_vtable *vt, const pp_vtable *pp_vt,
                 const encoding *x, const public_params *pp)
{
    if (vt->is_zero(pp_vt, x, pp) == ERR)
        return ERR;
    else
        return vt->mmap->enc->is_zero(x->enc, pp->pp);
}

encoding *
encoding_fread(const encoding_vtable *vt, FILE *fp)
{
    encoding *const x = my_calloc(1, sizeof(encoding));
    vt->fread(x, fp);
    x->enc = calloc(1, vt->mmap->enc->size);
    vt->mmap->enc->fread(x->enc, fp);
    return x;
}

void
encoding_fwrite(const encoding_vtable *vt, const encoding *x, FILE *fp)
{
    vt->fwrite(x, fp);
    vt->mmap->enc->fwrite(x->enc, fp);
}
