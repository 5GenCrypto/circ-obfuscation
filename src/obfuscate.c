#include "obfuscate.h"

#include "util.h"
#include <assert.h>
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>

static void
encode_v_ib(const mmap_vtable *mmap, encoding *enc, secret_params *p, mpz_t *rs,
            size_t i, bool b)
{
    level *lvl;

    lvl = level_create_v_ib(p->op, i, b);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_ib_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                   mpz_t *rs, size_t i, bool b)
{
    level *lvl;

    lvl = level_create_v_ib(p->op, i, b);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_ib_o(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                  mpz_t *rs, size_t i, bool b, size_t o)
{
    level *lvl;

    lvl = level_create_v_hat_ib_o(p->op, i, b, o);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_ib_o_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                         mpz_t *rs, size_t i, bool b, size_t o)
{
    level *lvl;

    lvl = level_create_v_hat_ib_o(p->op, i, b, o);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_i(const mmap_vtable *mmap, encoding *enc, secret_params *p, mpz_t *rs,
            size_t i)
{
    level *lvl;

    lvl = level_create_v_i(p->op, p->op->n + i);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_i_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                  mpz_t *rs, size_t i)
{
    level *lvl;

    lvl = level_create_v_i(p->op, p->op->n + i);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_o(const mmap_vtable *mmap, encoding *enc, secret_params *p,
               mpz_t *rs, size_t o)
{
    level *lvl;
    lvl = level_create_v_hat_o(p->op, o);
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

static void
encode_v_hat_o_v_star(const mmap_vtable *mmap, encoding *enc, secret_params *p,
                      mpz_t *rs, size_t o)
{
    level *lvl;

    lvl = level_create_v_hat_o(p->op, o);
    if (!p->op->simple) {
        level *v_star;

        v_star = level_create_v_star(p->op);
        level_mul_ui(v_star, v_star, p->op->D);
        level_add(lvl, lvl, v_star);
        level_free(v_star);
    }
    encode(mmap, enc, rs, p->op->nslots, lvl, p);
    level_free(lvl);
}

obfuscation *
obfuscation_new(const mmap_vtable *mmap, public_params *p)
{
    obf_params *op;
    obfuscation *obf;

    obf = malloc(sizeof(obfuscation));

    obf->mmap = mmap;
    obf->op = p->op;
    op = obf->op;

    obf->R_ib = lin_malloc(op->n * sizeof(encoding**));
    for (size_t i = 0; i < op->n; i++) {
        obf->R_ib[i] = lin_malloc(2 * sizeof(encoding*));
        for (size_t b = 0; b < 2; b++) {
            obf->R_ib[i][b] = encoding_new(mmap, p);
        }
    }
    obf->Z_ib = lin_malloc(op->n * sizeof(encoding**));
    for (size_t i = 0; i < op->n; i++) {
        obf->Z_ib[i] = lin_malloc(2 * sizeof(encoding*));
        for (size_t b = 0; b < 2; b++) {
            obf->Z_ib[i][b] = encoding_new(mmap, p);
        }
    }
    obf->R_hat_ib_o = lin_malloc(op->gamma * sizeof(encoding***));
    for (size_t o = 0; o < op->gamma; ++o) {
        obf->R_hat_ib_o[o] = lin_malloc(op->n * sizeof(encoding*));
        for (size_t i = 0; i < op->n; i++) {
            obf->R_hat_ib_o[o][i] = lin_malloc(2 * sizeof(encoding*));
            for (size_t b = 0; b < 2; b++) {
                obf->R_hat_ib_o[o][i][b] = encoding_new(mmap, p);
            }
        }
    }
    obf->Z_hat_ib_o = lin_malloc(op->gamma * sizeof(encoding***));
    for (size_t o = 0; o < op->gamma; ++o) {
        obf->Z_hat_ib_o[o] = lin_malloc(op->n * sizeof(encoding*));
        for (size_t i = 0; i < op->n; i++) {
            obf->Z_hat_ib_o[o][i] = lin_malloc(2 * sizeof(encoding*));
            for (size_t b = 0; b < 2; b++) {
                obf->Z_hat_ib_o[o][i][b] = encoding_new(mmap, p);
            }
        }
    }

    obf->R_i = lin_malloc(op->m * sizeof(encoding*));
    for (size_t i = 0; i < op->m; i++) {
        obf->R_i[i] = encoding_new(mmap, p);
    }

    obf->Z_i = lin_malloc(op->m * sizeof(encoding*));
    for (size_t i = 0; i < op->m; i++) {
        obf->Z_i[i] = encoding_new(mmap, p);
    }

    obf->R_o_i = lin_malloc(op->gamma * sizeof(encoding*));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->R_o_i[i] = encoding_new(mmap, p);
    }
    obf->Z_o_i = lin_malloc(op->gamma * sizeof(encoding*));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Z_o_i[i] = encoding_new(mmap, p);
    }

    return obf;
}

void
obfuscation_free(obfuscation *obf)
{
    /* obf_params *op = obf->op; */

    /* for (size_t i = 0; i < op->n; i++) { */
    /*     for (size_t b = 0; b < 2; b++) { */
    /*         encoding_free(obf->mmap, obf->R_ib[i][b]); */
    /*     } */
    /*     free(obf->R_ib[i]); */
    /* } */
    /* free(obf->R_ib); */
    /* for (size_t i = 0; i < op->n; i++) { */
    /*     for (size_t b = 0; b < 2; b++) { */
    /*         encoding_free(obf->mmap, obf->Z_ib[i][b]); */
    /*     } */
    /*     free(obf->Z_ib[i]); */
    /* } */
    /* free(obf->Z_ib); */
    /* for (size_t i = 0; i < op->n; i++) { */
    /*     for (size_t b = 0; b < 2; b++) { */
    /*         encoding_free(obf->mmap, obf->R_hat_ib[i][b]); */
    /*     } */
    /*     free(obf->R_hat_ib[i]); */
    /* } */
    /* free(obf->R_hat_ib); */
    /* for (size_t i = 0; i < op->n; i++) { */
    /*     for (size_t b = 0; b < 2; b++) { */
    /*         encoding_free(obf->mmap, obf->Z_hat_ib[i][b]); */
    /*     } */
    /*     free(obf->Z_hat_ib[i]); */
    /* } */
    /* free(obf->Z_hat_ib); */
    /* for (size_t i = 0; i < op->m; i++) { */
    /*     encoding_free(obf->mmap, obf->R_i[i]); */
    /* } */
    /* free(obf->R_i); */
    /* for (size_t i = 0; i < op->m; i++) { */
    /*     encoding_free(obf->mmap, obf->Z_i[i]); */
    /* } */
    /* free(obf->Z_i); */
    /* for (size_t i = 0; i < op->gamma; i++) { */
    /*     encoding_free(obf->mmap, obf->R_o_i[i]); */
    /* } */
    /* free(obf->R_o_i); */
    /* for (size_t i = 0; i < op->gamma; i++) { */
    /*     encoding_free(obf->mmap, obf->Z_o_i[i]); */
    /* } */
    /* free(obf->Z_o_i); */

    free(obf);
}

void
obfuscate(obfuscation *obf, secret_params *p, aes_randstate_t rng)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(obf->mmap->sk->plaintext_fields(p->sk),
                                            obf->mmap->sk->nslots(p->sk));
    size_t n = p->op->n, m = p->op->m, gamma = p->op->gamma;
    size_t nslots = obf->op->simple ? 2 : (n + 2);
    mpz_t *ys, *w_hats[n];

    obf->op = p->op;

    ys = mpz_vect_create(obf->op->n + obf->op->m);
    for (size_t i = 0; i < n; ++i) {
        w_hats[i] = mpz_vect_create(nslots);
        mpz_urandomm_vect_aes(w_hats[i], moduli, nslots, rng);
        if (!obf->op->simple) {
            mpz_set_ui(w_hats[i][i + 2], 0);
        }
    }

    /* encode R_ib and Z_ib */
    for (size_t i = 0; i < n; i++) {
        mpz_urandomm_aes(ys[i], rng, moduli[0]);
        for (size_t b = 0; b < 2; b++) {
            mpz_t *tmp, *other;

            tmp = mpz_vect_create(nslots);
            mpz_urandomm_vect_aes(tmp, moduli, nslots, rng);
            encode_v_ib(obf->mmap, obf->R_ib[i][b], p, tmp, i, b);

            other = mpz_vect_create(nslots);
            mpz_urandomm_vect_aes(other, moduli, nslots, rng);
            mpz_set_ui(other[1], b);
            mpz_set(other[0], ys[i]);
            mpz_vect_mul(tmp, tmp, other, nslots);
            mpz_vect_mod(tmp, tmp, moduli, nslots);
            encode_v_ib_v_star(obf->mmap, obf->Z_ib[i][b], p, tmp, i, b);

            mpz_vect_destroy(tmp, nslots);
            mpz_vect_destroy(other, nslots);
        }
    }

    /* encode R_hat_ib and Z_hat_ib */
    for (size_t o = 0; o < gamma; ++o) {
        for (size_t i = 0; i < n; i++) {
            for (size_t b = 0; b < 2; b++) {
                mpz_t *tmp;

                tmp = mpz_vect_create(nslots);
                mpz_urandomm_vect_aes(tmp, moduli, nslots, rng);
                encode_v_hat_ib_o(obf->mmap, obf->R_hat_ib_o[o][i][b], p, tmp, i, b, o);

                mpz_vect_mul(tmp, tmp, w_hats[i], nslots);
                mpz_vect_mod(tmp, tmp, moduli, nslots);

                encode_v_hat_ib_o_v_star(obf->mmap, obf->Z_hat_ib_o[o][i][b], p, tmp, i, b, o);

                mpz_vect_destroy(tmp, nslots);
            }
        }
    }

    /* encode R_i and Z_i */
    assert(obf->op->circ->nconsts == m);
    for (size_t i = 0; i < m; i++) {
        mpz_t *tmp, *other;

        tmp = mpz_vect_create(nslots);
        mpz_urandomm_vect_aes(tmp, moduli, nslots, rng);
        encode_v_i(obf->mmap, obf->R_i[i], p, tmp, i);

        other = mpz_vect_create(nslots);
        mpz_urandomm_vect_aes(other, moduli, nslots, rng);
        mpz_set(other[0], ys[n + i]);
        mpz_set_ui(other[1], obf->op->circ->consts[i]);
        mpz_vect_mul(tmp, tmp, other, nslots);
        mpz_vect_mod(tmp, tmp, moduli, nslots);
        encode_v_i_v_star(obf->mmap, obf->Z_i[i], p, tmp, i);
        
        mpz_vect_destroy(tmp, nslots);
        mpz_vect_destroy(other, nslots);
    }

    /* encode R_o_i and Z_o_i */
    for (size_t i = 0; i < gamma; ++i) {
        mpz_t y_0;
        mpz_t *rs, *ws;

        mpz_init(y_0);

        rs = mpz_vect_create(nslots);
        ws = mpz_vect_create(nslots);

        mpz_urandomm_vect_aes(rs, moduli, nslots, rng);
        encode_v_hat_o(obf->mmap, obf->R_o_i[i], p, rs, i);

        acirc_eval_mpz_mod(y_0, obf->op->circ, obf->op->circ->outrefs[i], ys,
                           ys + n, moduli[0]);
        mpz_set(ws[0], y_0);
        mpz_set_ui(ws[1], 1);

        for (size_t i = 0; i < n; ++i) {
            mpz_vect_mul(rs, rs, w_hats[i], nslots);
            mpz_vect_mod(rs, rs, moduli, nslots);
        }
        mpz_vect_mul(rs, rs, ws, nslots);
        mpz_vect_mod(rs, rs, moduli, nslots);

        encode_v_hat_o_v_star(obf->mmap, obf->Z_o_i[i], p, rs, i);
        
        mpz_vect_destroy(rs, nslots);
        mpz_vect_destroy(ws, nslots);
        mpz_clear(y_0);
    }

    mpz_vect_destroy(moduli, obf->mmap->sk->nslots(p->sk));
}

////////////////////////////////////////////////////////////////////////////////
// serialization

void obfuscation_eq (const obfuscation *x, const obfuscation *y)
{
    assert(false);
    /* (void) y; */
    /* obf_params *op = x->op; */
    /* assert(encoding_eq(x->Zstar, y->Zstar)); */
    /* for (size_t k = 0; k < op->c; k++) { */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         assert(encoding_eq(x->Rks[k][s], y->Rks[k][s])); */
    /*     } */
    /* } */

    /* for (size_t k = 0; k < op->c; k++) { */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         for (size_t j = 0; j < op->ell; j++) { */
    /*             assert(encoding_eq(x->Zksj[k][s][j], y->Zksj[k][s][j])); */
    /*         } */
    /*     } */
    /* } */

    /* assert(encoding_eq(x->Rc, y->Rc)); */
    /* for (size_t j = 0; j < op->m; j++) { */
    /*     assert(encoding_eq(x->Zcj[j], y->Zcj[j])); */
    /* } */

    /* for (size_t k = 0; k < op->c; k++) { */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         for (size_t o = 0; o < op->gamma; o++) { */
    /*             assert(encoding_eq(x->Rhatkso[k][s][o], y->Rhatkso[k][s][o])); */
    /*             assert(encoding_eq(x->Zhatkso[k][s][o], y->Zhatkso[k][s][o])); */
    /*         } */
    /*     } */
    /* } */

    /* for (size_t o = 0; o < op->gamma; o++) { */
    /*     assert(encoding_eq(x->Rhato[o], y->Rhato[o])); */
    /*     assert(encoding_eq(x->Zhato[o], y->Zhato[o])); */
    /* } */

    /* for (size_t o = 0; o < op->gamma; o++) { */
    /*     assert(encoding_eq(x->Rbaro[o], y->Rbaro[o])); */
    /*     assert(encoding_eq(x->Zbaro[o], y->Zbaro[o])); */
    /* } */
}

void
obfuscation_write(const obfuscation *obf, FILE *const fp)
{
    assert(false);
    /* obf_params *op = obf->op; */

    /* encoding_write(obf->mmap, obf->Zstar, fp); */
    /* (void) PUT_NEWLINE(fp); */

    /* for (size_t k = 0; k < op->c; k++) { */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         encoding_write(obf->mmap, obf->Rks[k][s], fp); */
    /*         (void) PUT_NEWLINE(fp); */
    /*     } */
    /* } */

    /* for (size_t k = 0; k < op->c; k++) { */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         for (size_t j = 0; j < op->ell; j++) { */
    /*             encoding_write(obf->mmap, obf->Zksj[k][s][j], fp); */
    /*             (void) PUT_NEWLINE(fp); */
    /*         } */
    /*     } */
    /* } */

    /* encoding_write(obf->mmap, obf->Rc, fp); */
    /* (void) PUT_NEWLINE(fp); */
    /* for (size_t j = 0; j < op->m; j++) { */
    /*     encoding_write(obf->mmap, obf->Zcj[j], fp); */
    /*     (void) PUT_NEWLINE(fp); */
    /* } */

    /* for (size_t k = 0; k < op->c; k++) { */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         for (size_t o = 0; o < op->gamma; o++) { */
    /*             encoding_write(obf->mmap, obf->Rhatkso[k][s][o], fp); */
    /*             (void) PUT_NEWLINE(fp); */
    /*             encoding_write(obf->mmap, obf->Zhatkso[k][s][o], fp); */
    /*             (void) PUT_NEWLINE(fp); */
    /*         } */
    /*     } */
    /* } */

    /* for (size_t o = 0; o < op->gamma; o++) { */
    /*     encoding_write(obf->mmap, obf->Rhato[o], fp); */
    /*     (void) PUT_NEWLINE(fp); */
    /*     encoding_write(obf->mmap, obf->Zhato[o], fp); */
    /*     (void) PUT_NEWLINE(fp); */
    /* } */

    /* for (size_t o = 0; o < op->gamma; o++) { */
    /*     encoding_write(obf->mmap, obf->Rbaro[o], fp); */
    /*     (void) PUT_NEWLINE(fp); */
    /*     encoding_write(obf->mmap, obf->Zbaro[o], fp); */
    /*     (void) PUT_NEWLINE(fp); */
    /* } */
}

void
obfuscation_read(obfuscation *obf, FILE *const fp, obf_params *p)
{
    assert(false);
    /* obf->op = p; */
    /* obf_params *op = p; */

    /* obf->Zstar = lin_malloc(sizeof(encoding)); */
    /* encoding_read(obf->mmap, obf->Zstar, fp); */
    /* GET_NEWLINE(fp); */

    /* obf->Rks = lin_malloc(op->c * sizeof(encoding**)); */
    /* for (size_t k = 0; k < op->c; k++) { */
    /*     obf->Rks[k] = lin_malloc(op->q * sizeof(encoding*)); */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         obf->Rks[k][s] = lin_malloc(sizeof(encoding)); */
    /*         encoding_read(obf->mmap, obf->Rks[k][s], fp); */
    /*         GET_NEWLINE(fp); */
    /*     } */
    /* } */

    /* obf->Zksj = lin_malloc(op->c * sizeof(encoding***)); */
    /* for (size_t k = 0; k < op->c; k++) { */
    /*     obf->Zksj[k] = lin_malloc(op->q * sizeof(encoding**)); */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         obf->Zksj[k][s] = lin_malloc(op->ell * sizeof(encoding*)); */
    /*         for (size_t j = 0; j < op->ell; j++) { */
    /*             obf->Zksj[k][s][j] = lin_malloc(sizeof(encoding)); */
    /*             encoding_read(obf->mmap, obf->Zksj[k][s][j], fp); */
    /*             GET_NEWLINE(fp); */
    /*         } */
    /*     } */
    /* } */

    /* obf->Rc = lin_malloc(sizeof(encoding)); */
    /* encoding_read(obf->mmap, obf->Rc, fp); */
    /* GET_NEWLINE(fp); */
    /* obf->Zcj = lin_malloc(op->m * sizeof(encoding*)); */
    /* for (size_t j = 0; j < op->m; j++) { */
    /*     obf->Zcj[j] = lin_malloc(sizeof(encoding)); */
    /*     encoding_read(obf->mmap, obf->Zcj[j], fp); */
    /*     GET_NEWLINE(fp); */
    /* } */

    /* obf->Rhatkso = lin_malloc(op->c * sizeof(encoding***)); */
    /* obf->Zhatkso = lin_malloc(op->c * sizeof(encoding***)); */
    /* for (size_t k = 0; k < op->c; k++) { */
    /*     obf->Rhatkso[k] = lin_malloc(op->q * sizeof(encoding**)); */
    /*     obf->Zhatkso[k] = lin_malloc(op->q * sizeof(encoding**)); */
    /*     for (size_t s = 0; s < op->q; s++) { */
    /*         obf->Rhatkso[k][s] = lin_malloc(op->gamma * sizeof(encoding*)); */
    /*         obf->Zhatkso[k][s] = lin_malloc(op->gamma * sizeof(encoding*)); */
    /*         for (size_t o = 0; o < op->gamma; o++) { */
    /*             obf->Rhatkso[k][s][o] = lin_malloc(sizeof(encoding)); */
    /*             obf->Zhatkso[k][s][o] = lin_malloc(sizeof(encoding)); */
    /*             encoding_read(obf->mmap, obf->Rhatkso[k][s][o], fp); */
    /*             GET_NEWLINE(fp); */
    /*             encoding_read(obf->mmap, obf->Zhatkso[k][s][o], fp); */
    /*             GET_NEWLINE(fp); */
    /*         } */
    /*     } */
    /* } */

    /* obf->Rhato = lin_malloc(op->gamma * sizeof(encoding*)); */
    /* obf->Zhato = lin_malloc(op->gamma * sizeof(encoding*)); */
    /* for (size_t o = 0; o < op->gamma; o++) { */
    /*     obf->Rhato[o] = lin_malloc(sizeof(encoding)); */
    /*     obf->Zhato[o] = lin_malloc(sizeof(encoding)); */
    /*     encoding_read(obf->mmap, obf->Rhato[o], fp); */
    /*     GET_NEWLINE(fp); */
    /*     encoding_read(obf->mmap, obf->Zhato[o], fp); */
    /*     GET_NEWLINE(fp); */
    /* } */

    /* obf->Rbaro = lin_malloc(op->gamma * sizeof(encoding*)); */
    /* obf->Zbaro = lin_malloc(op->gamma * sizeof(encoding*)); */
    /* for (size_t o = 0; o < op->gamma; o++) { */
    /*     obf->Rbaro[o] = lin_malloc(sizeof(encoding)); */
    /*     obf->Zbaro[o] = lin_malloc(sizeof(encoding)); */
    /*     encoding_read(obf->mmap, obf->Rbaro[o], fp); */
    /*     GET_NEWLINE(fp); */
    /*     encoding_read(obf->mmap, obf->Zbaro[o], fp); */
    /*     GET_NEWLINE(fp); */
    /* } */
}
