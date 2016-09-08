#include "obfuscate.h"

#include "util.h"
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static void
encode_Zstar(const mmap_vtable *mmap, encoding *enc, secret_params *p,
             aes_randstate_t rng)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(mmap->sk->plaintext_fields(p->sk),
                                            mmap->sk->nslots(p->sk));
    mpz_t *inps = mpz_vect_create(p->op->c+3);
    mpz_set_ui(inps[0], 1);
    mpz_set_ui(inps[1], 1);
    mpz_urandomm_vect_aes(inps + 2, moduli + 2, p->op->c+1, rng);

    {
        level *vstar = level_create_vstar(p->op);
        encode(mmap, enc, inps, p->op->c+3, vstar, p);
        level_free(vstar);
    }

    mpz_vect_destroy(moduli, mmap->sk->nslots(p->sk));
    mpz_vect_destroy(inps, p->op->c+3);
}

static void
encode_Rks(const mmap_vtable *mmap, encoding *enc, secret_params *p, mpz_t *rs,
           size_t k, size_t s)
{
    level *vsk = level_create_vks(p->op, k, s);
    encode(mmap, enc, rs, p->op->c+3, vsk, p);
    level_free(vsk);
}

static void
encode_Zksj(const mmap_vtable *mmap, encoding *enc, secret_params *p,
            aes_randstate_t rng, mpz_t *rs, mpz_t ykj, size_t k, size_t s,
            size_t j, bool rachel_input)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(mmap->sk->plaintext_fields(p->sk),
                                            mmap->sk->nslots(p->sk));
    mpz_t *w = mpz_vect_create(p->op->c+3);

    mpz_set(w[0], ykj);
    if (rachel_input)
        mpz_set_ui(w[1], s == j);
    else
        mpz_set_ui(w[1], bit(s,j));
    mpz_urandomm_vect_aes(w+2, moduli+2, p->op->c+1, rng);

    mpz_vect_mul(w, w, rs, p->op->c+3);
    mpz_vect_mod(w, w, moduli, p->op->c+3);

    level *lvl = level_create_vks(p->op, k, s);
    level *vstar = level_create_vstar(p->op);
    level_add(lvl, lvl, vstar);

    encode(mmap, enc, w, p->op->c+3, lvl, p);

    level_free(vstar);
    level_free(lvl);
    mpz_vect_destroy(w, p->op->c+3);
    mpz_vect_destroy(moduli, mmap->sk->nslots(p->sk));
}

static void
encode_Rc(const mmap_vtable *mmap, encoding *enc, secret_params *p, mpz_t *rs)
{
    level *vc = level_create_vc(p->op);
    encode(mmap, enc, rs, p->op->c+3, vc, p);
    level_free(vc);
}

static void
encode_Zcj(const mmap_vtable *mmap, encoding *enc, secret_params *p,
           aes_randstate_t rng, mpz_t *rs, mpz_t ykj, int const_val)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(mmap->sk->plaintext_fields(p->sk),
                                            mmap->sk->nslots(p->sk));
    mpz_t *w = mpz_vect_create(p->op->c+3);
    mpz_set(w[0], ykj);
    mpz_set_ui(w[1], const_val);
    mpz_urandomm_vect_aes(w+2, moduli+2, p->op->c+1, rng);

    mpz_vect_mul(w, w, rs, p->op->c+3);
    mpz_vect_mod(w, w, moduli, p->op->c+3);

    level *lvl = level_create_vc(p->op);
    level *vstar = level_create_vstar(p->op);
    level_add(lvl, lvl, vstar);

    encode(mmap, enc, w, p->op->c+3, lvl, p);

    level_free(vstar);
    level_free(lvl);
    mpz_vect_destroy(w, p->op->c+3);
    mpz_vect_destroy(moduli, mmap->sk->nslots(p->sk));
}

static void
encode_Rhatkso(const mmap_vtable *mmap, encoding *enc, secret_params *p,
               mpz_t *rs, size_t k, size_t s, size_t o)
{
    level *vhatkso = level_create_vhatkso(p->op, k, s, o);
    encode(mmap, enc, rs, p->op->c+3, vhatkso, p);
    level_free(vhatkso);
}

static void
encode_Zhatkso(const mmap_vtable *mmap, encoding *enc, secret_params *p,
               mpz_t *rs, mpz_t *whatk, size_t k, size_t s, size_t o)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(mmap->sk->plaintext_fields(p->sk),
                                            mmap->sk->nslots(p->sk));
    mpz_t *inp = mpz_vect_create(p->op->c+3);
    mpz_vect_mul(inp, whatk, rs, p->op->c+3);
    mpz_vect_mod(inp, inp, moduli, p->op->c+3);

    level *lvl   = level_create_vhatkso(p->op, k, s, o);
    level *vstar = level_create_vstar(p->op);
    level_add(lvl, lvl, vstar);

    encode(mmap, enc, inp, p->op->c+3, lvl, p);

    level_free(vstar);
    level_free(lvl);
    mpz_vect_destroy(inp, p->op->c+3);
    mpz_vect_destroy(moduli, mmap->sk->nslots(p->sk));
}

static void
encode_Rhato(const mmap_vtable *mmap, encoding *enc, secret_params *p,
             mpz_t *rs, size_t o)
{
    level *vhato = level_create_vhato(p->op, o);
    encode(mmap, enc, rs, p->op->c+3, vhato, p);
    level_free(vhato);
}

static void
encode_Zhato(const mmap_vtable *mmap, encoding *enc, secret_params *p,
             mpz_t *rs, mpz_t *what, size_t o)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(mmap->sk->plaintext_fields(p->sk),
                                            mmap->sk->nslots(p->sk));
    mpz_t *inp = mpz_vect_create(p->op->c+3);
    mpz_vect_mul(inp, what, rs, p->op->c+3);
    mpz_vect_mod(inp, inp, moduli, p->op->c+3);

    level *vhato = level_create_vhato(p->op, o);
    level *lvl = level_create_vstar(p->op);
    level_add(lvl, lvl, vhato);

    encode(mmap, enc, inp, p->op->c+3, lvl, p);

    level_free(lvl);
    level_free(vhato);
    mpz_vect_destroy(inp, p->op->c+3);
    mpz_vect_destroy(moduli, mmap->sk->nslots(p->sk));
}

static void
encode_Rbaro(const mmap_vtable *mmap, encoding *enc, secret_params *p,
             mpz_t *rs, size_t o)
{
    level *vbaro = level_create_vbaro(p->op, o);
    encode(mmap, enc, rs, p->op->c+3, vbaro, p);
    level_free(vbaro);
}

static void
encode_Zbaro(const mmap_vtable *mmap, encoding *enc, secret_params *p,
             mpz_t *rs, mpz_t *what, mpz_t **whatk, mpz_t **ykj, acirc *c,
             size_t o)
{
    mpz_t ybar;
    mpz_init(ybar);

    mpz_t *moduli = mpz_vect_create_of_fmpz(mmap->sk->plaintext_fields(p->sk),
                                            mmap->sk->nslots(p->sk));

    mpz_t *xs = mpz_vect_create(c->ninputs);
    for (size_t k = 0; k < p->op->c; k++) {
        for (size_t j = 0; j < p->op->ell; j++) {
            sym_id sym = {k, j};
            input_id id = p->op->rchunker(sym, c->ninputs, p->op->c);
            if (id >= c->ninputs)
                continue;
            mpz_set(xs[id], ykj[k][j]);
        }
    }
    acirc_eval_mpz_mod(ybar, c, c->outrefs[o], xs, ykj[p->op->c], moduli[0]);

    mpz_t *tmp = mpz_vect_create(p->op->c+3);
    mpz_vect_set(tmp, what, p->op->c+3);
    for (size_t k = 0; k < p->op->c; k++) {
        mpz_vect_mul(tmp, tmp, whatk[k], p->op->c+3);
        mpz_vect_mod(tmp, tmp, moduli, p->op->c+3);
    }

    mpz_t *w = mpz_vect_create(p->op->c+3);
    mpz_set(w[0], ybar);
    mpz_set_ui(w[1], 1);
    mpz_vect_mul(w, w, tmp, p->op->c+3);
    mpz_vect_mod(w, w, moduli, p->op->c+3);

    mpz_vect_mul(w, w, rs, p->op->c+3);
    mpz_vect_mod(w, w, moduli, p->op->c+3);

    level *lvl   = level_create_vbaro(p->op, o);
    level *vstar = level_create_vstar(p->op);
    level_mul_ui(vstar, vstar, p->op->D);
    level_add(lvl, lvl, vstar);

    encode(mmap, enc, w, p->op->c+3, lvl, p);

    mpz_vect_destroy(moduli, mmap->sk->nslots(p->sk));
    mpz_clear(ybar);
    mpz_vect_destroy(tmp, p->op->c+3);
    mpz_vect_destroy(xs, c->ninputs);
    mpz_vect_destroy(w, p->op->c+3);
    level_free(vstar);
    level_free(lvl);
}

void
obfuscation_init(const mmap_vtable *mmap, obfuscation *obf, public_params *p)
{
    obf->mmap = mmap;
    obf->op = p->op;
    obf_params *op = obf->op;

    obf->Zstar = encoding_new(mmap, p, p->op);

    obf->Rks = lin_malloc(op->c * sizeof(encoding**));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rks[k] = lin_malloc(op->q * sizeof(encoding*));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rks[k][s] = encoding_new(mmap, p, p->op);
        }
    }

    obf->Zksj = lin_malloc(op->c * sizeof(encoding***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Zksj[k] = lin_malloc(op->q * sizeof(encoding**));
        for (size_t s = 0; s < op->q; s++) {
            obf->Zksj[k][s] = lin_malloc(op->ell * sizeof(encoding*));
            for (size_t j = 0; j < op->ell; j++) {
                obf->Zksj[k][s][j] = encoding_new(mmap, p, p->op);
            }
        }
    }

    obf->Rc = encoding_new(mmap, p, p->op);
    obf->Zcj = lin_malloc(op->m * sizeof(encoding*));
    for (size_t j = 0; j < op->m; j++) {
        obf->Zcj[j] = encoding_new(mmap, p, p->op);
    }

    obf->Rhatkso = lin_malloc(op->c * sizeof(encoding***));
    obf->Zhatkso = lin_malloc(op->c * sizeof(encoding***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rhatkso[k] = lin_malloc(op->q * sizeof(encoding**));
        obf->Zhatkso[k] = lin_malloc(op->q * sizeof(encoding**));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rhatkso[k][s] = lin_malloc(op->gamma * sizeof(encoding*));
            obf->Zhatkso[k][s] = lin_malloc(op->gamma * sizeof(encoding*));
            for (size_t o = 0; o < op->gamma; o++) {
                obf->Rhatkso[k][s][o] = encoding_new(mmap, p, p->op);
                obf->Zhatkso[k][s][o] = encoding_new(mmap, p, p->op);
            }
        }
    }

    obf->Rhato = lin_malloc(op->gamma * sizeof(encoding*));
    obf->Zhato = lin_malloc(op->gamma * sizeof(encoding*));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rhato[o] = encoding_new(mmap, p, p->op);
        obf->Zhato[o] = encoding_new(mmap, p, p->op);
    }

    obf->Rbaro = lin_malloc(op->gamma * sizeof(encoding*));
    obf->Zbaro = lin_malloc(op->gamma * sizeof(encoding*));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rbaro[o] = encoding_new(mmap, p, p->op);
        obf->Zbaro[o] = encoding_new(mmap, p, p->op);
    }
}

void
obfuscation_clear(obfuscation *obf)
{
    obf_params *op = obf->op;

    encoding_free(obf->mmap, obf->Zstar);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            encoding_free(obf->mmap, obf->Rks[k][s]);
        }
        free(obf->Rks[k]);
    }
    free(obf->Rks);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                encoding_free(obf->mmap, obf->Zksj[k][s][j]);
            }
            free(obf->Zksj[k][s]);
        }
        free(obf->Zksj[k]);
    }
    free(obf->Zksj);

    encoding_free(obf->mmap, obf->Rc);

    for (size_t j = 0; j < op->m; j++) {
        encoding_free(obf->mmap, obf->Zcj[j]);
    }
    free(obf->Zcj);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t o = 0; o < op->gamma; o++) {
                encoding_free(obf->mmap, obf->Rhatkso[k][s][o]);
                encoding_free(obf->mmap, obf->Zhatkso[k][s][o]);
            }
            free(obf->Rhatkso[k][s]);
            free(obf->Zhatkso[k][s]);
        }
        free(obf->Rhatkso[k]);
        free(obf->Zhatkso[k]);
    }
    free(obf->Rhatkso);
    free(obf->Zhatkso);

    for (size_t o = 0; o < op->gamma; o++) {
        encoding_free(obf->mmap, obf->Rhato[o]);
        encoding_free(obf->mmap, obf->Zhato[o]);
    }
    free(obf->Rhato);
    free(obf->Zhato);

    for (size_t o = 0; o < op->gamma; o++) {
        encoding_free(obf->mmap, obf->Rbaro[o]);
        encoding_free(obf->mmap, obf->Zbaro[o]);
    }
    free(obf->Rbaro);
    free(obf->Zbaro);
}

void
obfuscate(obfuscation *obf, secret_params *p, aes_randstate_t rng,
          bool rachel_input)
{
    mpz_t *moduli = mpz_vect_create_of_fmpz(obf->mmap->sk->plaintext_fields(p->sk),
                                            obf->mmap->sk->nslots(p->sk));
    obf->op = p->op;

    // create ykj
    mpz_t **ykj = lin_malloc((p->op->c+1) * sizeof(mpz_t*));
    for (size_t k = 0; k < p->op->c; k++) {
        ykj[k] = lin_malloc(p->op->ell * sizeof(mpz_t));
        for (size_t j = 0; j < p->op->ell; j++) {
            mpz_init(ykj[k][j]);
            mpz_urandomm_aes(ykj[k][j], rng, moduli[0]);
        }
    }
    // the cth ykj has length m (number of secret bits)
    ykj[p->op->c] = lin_malloc(p->op->m * sizeof(mpz_t));
    for (size_t j = 0; j < p->op->m; j++) {
        mpz_init(ykj[p->op->c][j]);
        mpz_urandomm_aes(ykj[p->op->c][j], rng, moduli[0]);
    }

    // create whatk and what
    mpz_t **whatk = lin_malloc((p->op->c+1) * sizeof(mpz_t*));
    for (size_t k = 0; k < p->op->c; k++) {
        whatk[k] = mpz_vect_create(p->op->c+3);
        mpz_urandomm_vect_aes(whatk[k], moduli, p->op->c+3, rng);
        mpz_set_ui(whatk[k][k+2], 0);
    }
    mpz_t *what = mpz_vect_create(p->op->c+3);
    mpz_urandomm_vect_aes(what, moduli, p->op->c+3, rng);
    mpz_set_ui(what[p->op->c+2], 0);

    ////////////////////////////////////////////////////////////////////////////////
    // obfuscation

    encode_Zstar(obf->mmap, obf->Zstar, p, rng);

    // encode Rks and Zksj
/* #pragma omp parallel for schedule(dynamic,1) collapse(2) */
    for (size_t k = 0; k < p->op->c; k++) {
        for (size_t s = 0; s < p->op->q; s++) {
            mpz_t *tmp = mpz_vect_create(p->op->c+3);
            mpz_urandomm_vect_aes(tmp, moduli, p->op->c+3, rng);
            encode_Rks(obf->mmap, obf->Rks[k][s], p, tmp, k, s);
            for (size_t j = 0; j < p->op->ell; j++) {
                encode_Zksj(obf->mmap, obf->Zksj[k][s][j], p, rng, tmp,
                            ykj[k][j], k, s, j, rachel_input);
            }
            mpz_vect_destroy(tmp, p->op->c+3);
        }
    }

    // encode Rc and Zcj
    {
        mpz_t *rs = mpz_vect_create(p->op->c+3);
        mpz_urandomm_vect_aes(rs, moduli, p->op->c+3, rng);
        encode_Rc(obf->mmap, obf->Rc, p, rs);
/* #pragma omp parallel for */
        for (size_t j = 0; j < p->op->m; j++) {
            encode_Zcj(obf->mmap, obf->Zcj[j], p, rng, rs, ykj[p->op->c][j],
                       p->op->circ->consts[j]);
        }
        mpz_vect_destroy(rs, p->op->c+3);
    }

    // encode Rhatkso and Zhatkso
/* #pragma omp parallel for schedule(dynamic,1) collapse(3) */
    for (size_t o = 0; o < p->op->gamma; o++) {
        for (size_t k = 0; k < p->op->c; k++) {
            for (size_t s = 0; s < p->op->q; s++) {
                mpz_t *tmp = mpz_vect_create(p->op->c+3);
                mpz_urandomm_vect_aes(tmp, moduli, p->op->c+3, rng);
                encode_Rhatkso(obf->mmap, obf->Rhatkso[k][s][o], p, tmp, k, s, o);
                encode_Zhatkso(obf->mmap, obf->Zhatkso[k][s][o], p, tmp, whatk[k], k, s, o);
                mpz_vect_destroy(tmp, p->op->c+3);
            }
        }
    }

    // encode Rhato and Zhato
/* #pragma omp parallel for */
    for (size_t o = 0; o < p->op->gamma; o++) {
        mpz_t *tmp = mpz_vect_create(p->op->c+3);
        mpz_urandomm_vect_aes(tmp, moduli, p->op->c+3, rng);
        encode_Rhato(obf->mmap, obf->Rhato[o], p, tmp, o);
        encode_Zhato(obf->mmap, obf->Zhato[o], p, tmp, what, o);
        mpz_vect_destroy(tmp, p->op->c+3);
    }

    // encode Rbaro and Zbaro
/* #pragma omp parallel for */
    for (size_t o = 0; o < p->op->gamma; o++) {
        mpz_t *tmp = mpz_vect_create(p->op->c+3);
        mpz_urandomm_vect_aes(tmp, moduli, p->op->c+3, rng);
        encode_Rbaro(obf->mmap, obf->Rbaro[o], p, tmp, o);
        encode_Zbaro(obf->mmap, obf->Zbaro[o], p, tmp, what, whatk, ykj, p->op->circ, o);
        mpz_vect_destroy(tmp, p->op->c+3);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // cleanup

    // delete ykj
    for (size_t k = 0; k < p->op->c; k++) {
        for (size_t j = 0; j < p->op->ell; j++)
            mpz_clear(ykj[k][j]);
        free(ykj[k]);
    }
    for (size_t j = 0; j < p->op->m; j++)
        mpz_clear(ykj[p->op->c][j]);
    free(ykj[p->op->c]);
    free(ykj);

    // delete whatk and what
    for (size_t k = 0; k < p->op->c; k++) {
        mpz_vect_destroy(whatk[k], p->op->c+3);
    }
    free(whatk);
    mpz_vect_destroy(what, p->op->c+3);
    mpz_vect_destroy(moduli, obf->mmap->sk->nslots(p->sk));
}

////////////////////////////////////////////////////////////////////////////////
// serialization

void obfuscation_eq (const obfuscation *x, const obfuscation *y)
{
    (void) y;
    obf_params *op = x->op;
    assert(encoding_eq(x->Zstar, y->Zstar));
    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            assert(encoding_eq(x->Rks[k][s], y->Rks[k][s]));
        }
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                assert(encoding_eq(x->Zksj[k][s][j], y->Zksj[k][s][j]));
            }
        }
    }

    assert(encoding_eq(x->Rc, y->Rc));
    for (size_t j = 0; j < op->m; j++) {
        assert(encoding_eq(x->Zcj[j], y->Zcj[j]));
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t o = 0; o < op->gamma; o++) {
                assert(encoding_eq(x->Rhatkso[k][s][o], y->Rhatkso[k][s][o]));
                assert(encoding_eq(x->Zhatkso[k][s][o], y->Zhatkso[k][s][o]));
            }
        }
    }

    for (size_t o = 0; o < op->gamma; o++) {
        assert(encoding_eq(x->Rhato[o], y->Rhato[o]));
        assert(encoding_eq(x->Zhato[o], y->Zhato[o]));
    }

    for (size_t o = 0; o < op->gamma; o++) {
        assert(encoding_eq(x->Rbaro[o], y->Rbaro[o]));
        assert(encoding_eq(x->Zbaro[o], y->Zbaro[o]));
    }
}

void
obfuscation_write(const obfuscation *obf, FILE *const fp)
{
    obf_params *op = obf->op;

    encoding_write(obf->mmap, obf->Zstar, fp);
    (void) PUT_NEWLINE(fp);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            encoding_write(obf->mmap, obf->Rks[k][s], fp);
            (void) PUT_NEWLINE(fp);
        }
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                encoding_write(obf->mmap, obf->Zksj[k][s][j], fp);
                (void) PUT_NEWLINE(fp);
            }
        }
    }

    encoding_write(obf->mmap, obf->Rc, fp);
    (void) PUT_NEWLINE(fp);
    for (size_t j = 0; j < op->m; j++) {
        encoding_write(obf->mmap, obf->Zcj[j], fp);
        (void) PUT_NEWLINE(fp);
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t o = 0; o < op->gamma; o++) {
                encoding_write(obf->mmap, obf->Rhatkso[k][s][o], fp);
                (void) PUT_NEWLINE(fp);
                encoding_write(obf->mmap, obf->Zhatkso[k][s][o], fp);
                (void) PUT_NEWLINE(fp);
            }
        }
    }

    for (size_t o = 0; o < op->gamma; o++) {
        encoding_write(obf->mmap, obf->Rhato[o], fp);
        (void) PUT_NEWLINE(fp);
        encoding_write(obf->mmap, obf->Zhato[o], fp);
        (void) PUT_NEWLINE(fp);
    }

    for (size_t o = 0; o < op->gamma; o++) {
        encoding_write(obf->mmap, obf->Rbaro[o], fp);
        (void) PUT_NEWLINE(fp);
        encoding_write(obf->mmap, obf->Zbaro[o], fp);
        (void) PUT_NEWLINE(fp);
    }
}

void
obfuscation_read(obfuscation *obf, FILE *const fp, obf_params *p)
{
    obf->op = p;
    obf_params *op = p;

    obf->Zstar = lin_malloc(sizeof(encoding));
    encoding_read(obf->mmap, obf->Zstar, fp);
    GET_NEWLINE(fp);

    obf->Rks = lin_malloc(op->c * sizeof(encoding**));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rks[k] = lin_malloc(op->q * sizeof(encoding*));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rks[k][s] = lin_malloc(sizeof(encoding));
            encoding_read(obf->mmap, obf->Rks[k][s], fp);
            GET_NEWLINE(fp);
        }
    }

    obf->Zksj = lin_malloc(op->c * sizeof(encoding***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Zksj[k] = lin_malloc(op->q * sizeof(encoding**));
        for (size_t s = 0; s < op->q; s++) {
            obf->Zksj[k][s] = lin_malloc(op->ell * sizeof(encoding*));
            for (size_t j = 0; j < op->ell; j++) {
                obf->Zksj[k][s][j] = lin_malloc(sizeof(encoding));
                encoding_read(obf->mmap, obf->Zksj[k][s][j], fp);
                GET_NEWLINE(fp);
            }
        }
    }

    obf->Rc = lin_malloc(sizeof(encoding));
    encoding_read(obf->mmap, obf->Rc, fp);
    GET_NEWLINE(fp);
    obf->Zcj = lin_malloc(op->m * sizeof(encoding*));
    for (size_t j = 0; j < op->m; j++) {
        obf->Zcj[j] = lin_malloc(sizeof(encoding));
        encoding_read(obf->mmap, obf->Zcj[j], fp);
        GET_NEWLINE(fp);
    }

    obf->Rhatkso = lin_malloc(op->c * sizeof(encoding***));
    obf->Zhatkso = lin_malloc(op->c * sizeof(encoding***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rhatkso[k] = lin_malloc(op->q * sizeof(encoding**));
        obf->Zhatkso[k] = lin_malloc(op->q * sizeof(encoding**));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rhatkso[k][s] = lin_malloc(op->gamma * sizeof(encoding*));
            obf->Zhatkso[k][s] = lin_malloc(op->gamma * sizeof(encoding*));
            for (size_t o = 0; o < op->gamma; o++) {
                obf->Rhatkso[k][s][o] = lin_malloc(sizeof(encoding));
                obf->Zhatkso[k][s][o] = lin_malloc(sizeof(encoding));
                encoding_read(obf->mmap, obf->Rhatkso[k][s][o], fp);
                GET_NEWLINE(fp);
                encoding_read(obf->mmap, obf->Zhatkso[k][s][o], fp);
                GET_NEWLINE(fp);
            }
        }
    }

    obf->Rhato = lin_malloc(op->gamma * sizeof(encoding*));
    obf->Zhato = lin_malloc(op->gamma * sizeof(encoding*));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rhato[o] = lin_malloc(sizeof(encoding));
        obf->Zhato[o] = lin_malloc(sizeof(encoding));
        encoding_read(obf->mmap, obf->Rhato[o], fp);
        GET_NEWLINE(fp);
        encoding_read(obf->mmap, obf->Zhato[o], fp);
        GET_NEWLINE(fp);
    }

    obf->Rbaro = lin_malloc(op->gamma * sizeof(encoding*));
    obf->Zbaro = lin_malloc(op->gamma * sizeof(encoding*));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rbaro[o] = lin_malloc(sizeof(encoding));
        obf->Zbaro[o] = lin_malloc(sizeof(encoding));
        encoding_read(obf->mmap, obf->Rbaro[o], fp);
        GET_NEWLINE(fp);
        encoding_read(obf->mmap, obf->Zbaro[o], fp);
        GET_NEWLINE(fp);
    }
}
