#include "vtables.h"

#include "obfuscator.h"
#include "obf_params.h"

#include "../util.h"

#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

struct obfuscation {
    const mmap_vtable *mmap;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    const encoding_vtable *enc_vt;
    const obf_params_t *op;
    secret_params *sp;
    public_params *pp;
    aes_randstate_t rng;
    encoding *Zstar;
    encoding ***Rks;        // k \in [c], s \in \Sigma
    encoding ****Zksj;      // k \in [c], s \in \Sigma, j \in [\ell]
    encoding *Rc;
    encoding **Zcj;         // j \in [m] where m is length of secret P
    encoding ****Rhatkso;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding ****Zhatkso;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding **Rhato;       // o \in \Gamma
    encoding **Zhato;       // o \in \Gamma
    encoding **Rbaro;       // o \in \Gamma
    encoding **Zbaro;       // o \in \Gamma
};


static void
encode_Zstar(const encoding_vtable *const vt, const obf_params_t *const op,
             encoding *const enc, const secret_params *const sp,
             aes_randstate_t rng, const mpz_t *const moduli)
{
    mpz_t *inps = mpz_vect_new(op->c+3);
    mpz_set_ui(inps[0], 1);
    mpz_set_ui(inps[1], 1);
    mpz_vect_urandomms(inps + 2, moduli + 2, op->c+1, rng);

    {
        level *lvl = level_create_vstar(op);
        encode(vt, enc, inps, op->c+3, lvl, sp);
        level_free(lvl);
    }

    mpz_vect_free(inps, op->c+3);
}

static void
encode_Rks(const encoding_vtable *const vt, const obf_params_t *const op,
           encoding *const enc, const secret_params *const sp, mpz_t *rs,
           size_t k, size_t s)
{
    level *lvl = level_create_vks(op, k, s);
    encode(vt, enc, rs, op->c+3, lvl, sp);
    level_free(lvl);
}

static void
encode_Zksj(const encoding_vtable *const vt, const obf_params_t *const op,
            encoding *const enc, const secret_params *const sp,
            aes_randstate_t rng, mpz_t *rs, mpz_t ykj, size_t k, size_t s,
            size_t j, bool rachel_input, const mpz_t *const moduli)
{
    mpz_t *w = mpz_vect_new(op->c+3);

    mpz_set(w[0], ykj);
    if (rachel_input)
        mpz_set_ui(w[1], s == j);
    else
        mpz_set_ui(w[1], bit(s,j));
    mpz_vect_urandomms(w+2, moduli+2, op->c+1, rng);

    mpz_vect_mul(w, (const mpz_t *) w, (const mpz_t *) rs, op->c+3);
    mpz_vect_mod(w, (const mpz_t *) w, moduli, op->c+3);

    level *lvl = level_create_vks(op, k, s);
    level *vstar = level_create_vstar(op);
    level_add(lvl, lvl, vstar);

    encode(vt, enc, w, op->c+3, lvl, sp);

    level_free(vstar);
    level_free(lvl);
    mpz_vect_free(w, op->c+3);
}

static void
encode_Rc(const encoding_vtable *const vt, const obf_params_t *const op,
          encoding *const enc, const secret_params *const sp, mpz_t *const rs)
{
    level *lvl = level_create_vc(op);
    encode(vt, enc, rs, op->c+3, lvl, sp);
    level_free(lvl);
}

static void
encode_Zcj(const encoding_vtable *const vt, const obf_params_t *const op,
           encoding *const enc, const secret_params *const sp,
           aes_randstate_t rng, mpz_t *rs, mpz_t ykj, int const_val,
           const mpz_t *const moduli)
{
    mpz_t *w = mpz_vect_new(op->c+3);
    mpz_set(w[0], ykj);
    mpz_set_ui(w[1], const_val);
    mpz_vect_urandomms(w+2, moduli+2, op->c+1, rng);

    mpz_vect_mul(w, (const mpz_t *) w, (const mpz_t *) rs, op->c+3);
    mpz_vect_mod(w, (const mpz_t *) w, moduli, op->c+3);

    level *lvl = level_create_vc(op);
    level *vstar = level_create_vstar(op);
    level_add(lvl, lvl, vstar);

    encode(vt, enc, w, op->c+3, lvl, sp);

    level_free(vstar);
    level_free(lvl);
    mpz_vect_free(w, op->c+3);
}

static void
encode_Rhatkso(const encoding_vtable *const vt, const obf_params_t *const op,
               encoding *const enc, const secret_params *const sp,
               mpz_t *const rs, size_t k, size_t s, size_t o)
{
    level *vhatkso = level_create_vhatkso(op, k, s, o);
    encode(vt, enc, rs, op->c+3, vhatkso, sp);
    level_free(vhatkso);
}

static void
encode_Zhatkso(const encoding_vtable *const vt, const obf_params_t *const op,
               encoding *const enc, const secret_params *const sp,
               const mpz_t *const rs, const mpz_t *const whatk, size_t k, size_t s, size_t o,
               const mpz_t *const moduli)
{
    mpz_t *inp = mpz_vect_new(op->c+3);
    mpz_vect_mul_mod(inp, whatk, rs, moduli, op->c+3);

    level *lvl   = level_create_vhatkso(op, k, s, o);
    level *vstar = level_create_vstar(op);
    level_add(lvl, lvl, vstar);

    encode(vt, enc, inp, op->c+3, lvl, sp);

    level_free(vstar);
    level_free(lvl);
    mpz_vect_free(inp, op->c+3);
}

static void
encode_Rhato(const encoding_vtable *const vt, const obf_params_t *const op,
             encoding *const enc, const secret_params *const sp,
             mpz_t *const rs, size_t o)
{
    level *vhato = level_create_vhato(op, o);
    encode(vt, enc, rs, op->c+3, vhato, sp);
    level_free(vhato);
}

static void
encode_Zhato(const encoding_vtable *const vt, const obf_params_t *const op,
             encoding *const enc, const secret_params *const sp,
             const mpz_t *const rs, const mpz_t *const what, size_t o,
             const mpz_t *const moduli)
{
    mpz_t *inp = mpz_vect_new(op->c+3);
    mpz_vect_mul_mod(inp, what, rs, moduli, op->c+3);

    level *vhato = level_create_vhato(op, o);
    level *lvl = level_create_vstar(op);
    level_add(lvl, lvl, vhato);

    encode(vt, enc, inp, op->c+3, lvl, sp);

    level_free(lvl);
    level_free(vhato);
    mpz_vect_free(inp, op->c+3);
}

static void
encode_Rbaro(const encoding_vtable *const vt, const obf_params_t *const op,
             encoding *const enc, const secret_params *const sp,
             mpz_t *const rs, size_t o)
{
    level *vbaro = level_create_vbaro(op, o);
    encode(vt, enc, rs, op->c+3, vbaro, sp);
    level_free(vbaro);
}

static void
encode_Zbaro(const encoding_vtable *const vt, const obf_params_t *const op,
             encoding *const enc, const secret_params *const sp,
             const mpz_t *const rs, const mpz_t *const what, const mpz_t *const *const whatk,
             const mpz_t *const *const ykj, acirc *const c,
             size_t o, const mpz_t *const moduli)
{
    mpz_t ybar;
    mpz_init(ybar);

    mpz_t *const xs = mpz_vect_new(c->ninputs);
    for (size_t k = 0; k < op->c; k++) {
        for (size_t j = 0; j < op->ell; j++) {
            sym_id sym = {k, j};
            input_id id = op->rchunker(sym, c->ninputs, op->c);
            if ((size_t) id >= c->ninputs)
                continue;
            mpz_set(xs[id], ykj[k][j]);
        }
    }
    acirc_eval_mpz_mod(ybar, c, c->outrefs[o], (const mpz_t *) xs, ykj[op->c], moduli[0]);

    mpz_t *tmp = mpz_vect_new(op->c+3);
    mpz_vect_set(tmp, what, op->c+3);
    for (size_t k = 0; k < op->c; k++) {
        mpz_vect_mul_mod(tmp, (const mpz_t *) tmp, whatk[k], moduli, op->c+3);
    }

    mpz_t *w = mpz_vect_new(op->c+3);
    mpz_set(w[0], ybar);
    mpz_set_ui(w[1], 1);
    mpz_vect_mul_mod(w, (const mpz_t *) w, (const mpz_t *) tmp, moduli, op->c+3);

    mpz_vect_mul_mod(w, (const mpz_t *) w, (const mpz_t *) rs, moduli, op->c+3);

    level *lvl   = level_create_vbaro(op, o);
    level *vstar = level_create_vstar(op);
    level_mul_ui(vstar, vstar, op->D);
    level_add(lvl, lvl, vstar);

    encode(vt, enc, w, op->c+3, lvl, sp);

    mpz_clear(ybar);
    mpz_vect_free(tmp, op->c+3);
    mpz_vect_free(xs, c->ninputs);
    mpz_vect_free(w, op->c+3);
    level_free(vstar);
    level_free(lvl);
}

static void
_obfuscation_free(obfuscation *obf);

static obfuscation *
_obfuscation_new(const mmap_vtable *const mmap, const obf_params_t *const op,
                 size_t secparam)
{
    obfuscation *obf;

    obf = my_calloc(1, sizeof(obfuscation));
    obf->mmap = mmap;
    obf->enc_vt = lin_get_encoding_vtable(mmap);
    obf->pp_vt = lin_get_pp_vtable(mmap);
    obf->sp_vt = lin_get_sp_vtable(mmap);
    obf->op = op;
    aes_randinit(obf->rng);
    obf->sp = my_calloc(1, sizeof(secret_params));
    if (secret_params_init(obf->sp_vt, obf->sp, op, secparam, obf->rng)) {
        _obfuscation_free(obf);
        return NULL;
    }
    obf->pp = my_calloc(1, sizeof(public_params));
    public_params_init(obf->pp_vt, obf->sp_vt, obf->pp, obf->sp);

    obf->Zstar = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

    obf->Rks = my_calloc(op->c, sizeof(encoding **));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rks[k] = my_calloc(op->q, sizeof(encoding *));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rks[k][s] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        }
    }

    obf->Zksj = my_calloc(op->c, sizeof(encoding ***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Zksj[k] = my_calloc(op->q, sizeof(encoding **));
        for (size_t s = 0; s < op->q; s++) {
            obf->Zksj[k][s] = my_calloc(op->ell, sizeof(encoding *));
            for (size_t j = 0; j < op->ell; j++) {
                obf->Zksj[k][s][j] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }

    obf->Rc = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    obf->Zcj = my_calloc(op->m, sizeof(encoding *));
    for (size_t j = 0; j < op->m; j++) {
        obf->Zcj[j] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }

    obf->Rhatkso = my_calloc(op->c, sizeof(encoding ***));
    obf->Zhatkso = my_calloc(op->c, sizeof(encoding ***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rhatkso[k] = my_calloc(op->q, sizeof(encoding **));
        obf->Zhatkso[k] = my_calloc(op->q, sizeof(encoding **));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rhatkso[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            obf->Zhatkso[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            for (size_t o = 0; o < op->gamma; o++) {
                obf->Rhatkso[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
                obf->Zhatkso[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }

    obf->Rhato = my_calloc(op->gamma, sizeof(encoding *));
    obf->Zhato = my_calloc(op->gamma, sizeof(encoding *));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rhato[o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        obf->Zhato[o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }

    obf->Rbaro = my_calloc(op->gamma, sizeof(encoding *));
    obf->Zbaro = my_calloc(op->gamma, sizeof(encoding *));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rbaro[o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        obf->Zbaro[o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }

    return obf;
}

static void
_obfuscation_free(obfuscation *obf)
{
    if (obf == NULL)
        return;
    
    const obf_params_t *const op = obf->op;

    if (obf->Zstar)
        encoding_free(obf->enc_vt, obf->Zstar);

    if (obf->Rks) {
        for (size_t k = 0; k < op->c; k++) {
            for (size_t s = 0; s < op->q; s++) {
                encoding_free(obf->enc_vt, obf->Rks[k][s]);
            }
            free(obf->Rks[k]);
        }
        free(obf->Rks);
    }

    if (obf->Zksj) {
        for (size_t k = 0; k < op->c; k++) {
            for (size_t s = 0; s < op->q; s++) {
                for (size_t j = 0; j < op->ell; j++) {
                    encoding_free(obf->enc_vt, obf->Zksj[k][s][j]);
                }
                free(obf->Zksj[k][s]);
            }
            free(obf->Zksj[k]);
        }
        free(obf->Zksj);
    }

    if (obf->Rc)
        encoding_free(obf->enc_vt, obf->Rc);

    if (obf->Zcj) {
        for (size_t j = 0; j < op->m; j++) {
            encoding_free(obf->enc_vt, obf->Zcj[j]);
        }
        free(obf->Zcj);
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t o = 0; o < op->gamma; o++) {
                encoding_free(obf->enc_vt, obf->Rhatkso[k][s][o]);
                encoding_free(obf->enc_vt, obf->Zhatkso[k][s][o]);
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
        encoding_free(obf->enc_vt, obf->Rhato[o]);
        encoding_free(obf->enc_vt, obf->Zhato[o]);
    }
    free(obf->Rhato);
    free(obf->Zhato);

    for (size_t o = 0; o < op->gamma; o++) {
        encoding_free(obf->enc_vt, obf->Rbaro[o]);
        encoding_free(obf->enc_vt, obf->Zbaro[o]);
    }
    free(obf->Rbaro);
    free(obf->Zbaro);
}

static int
_obfuscate(obfuscation *const obf)
{
    mpz_t *const moduli =
        mpz_vect_create_of_fmpz(obf->mmap->sk->plaintext_fields(obf->sp->sk),
                                obf->mmap->sk->nslots(obf->sp->sk));
    const obf_params_t *const op = obf->op;

    // create ykj
    mpz_t **ykj = my_calloc(op->c+1, sizeof(mpz_t *));
    for (size_t k = 0; k < op->c; k++) {
        ykj[k] = my_calloc(op->ell, sizeof(mpz_t));
        for (size_t j = 0; j < op->ell; j++) {
            mpz_init(ykj[k][j]);
            mpz_urandomm_aes(ykj[k][j], obf->rng, moduli[0]);
        }
    }
    // the cth ykj has length m (number of secret bits)
    ykj[op->c] = my_calloc(op->m, sizeof(mpz_t));
    for (size_t j = 0; j < op->m; j++) {
        mpz_init(ykj[op->c][j]);
        mpz_urandomm_aes(ykj[op->c][j], obf->rng, moduli[0]);
    }

    // create whatk and what
    mpz_t **const whatk = my_calloc(op->c, sizeof(mpz_t *));
    for (size_t k = 0; k < op->c; k++) {
        whatk[k] = mpz_vect_new(op->c+3);
        mpz_vect_urandomms(whatk[k], (const mpz_t *) moduli, op->c+3, obf->rng);
        mpz_set_ui(whatk[k][k+2], 0);
    }
    mpz_t *const what = mpz_vect_new(op->c+3);
    mpz_vect_urandomms(what, (const mpz_t *) moduli, op->c+3, obf->rng);
    mpz_set_ui(what[op->c+2], 0);

    encode_Zstar(obf->enc_vt, obf->op, obf->Zstar, obf->sp, obf->rng, (const mpz_t *) moduli);

    // encode Rks and Zksj
/* #pragma omp parallel for schedule(dynamic,1) collapse(2) */
    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            mpz_t *tmp = mpz_vect_new(op->c+3);
            mpz_vect_urandomms(tmp, (const mpz_t *) moduli, op->c+3, obf->rng);
            encode_Rks(obf->enc_vt, op, obf->Rks[k][s], obf->sp, tmp, k, s);
            for (size_t j = 0; j < op->ell; j++) {
                encode_Zksj(obf->enc_vt, op, obf->Zksj[k][s][j], obf->sp,
                            obf->rng, tmp, ykj[k][j], k, s, j, op->rachel_input,
                            (const mpz_t *) moduli);
            }
            mpz_vect_free(tmp, op->c+3);
        }
    }

    // encode Rc and Zcj
    {
        mpz_t *rs = mpz_vect_new(op->c+3);
        mpz_vect_urandomms(rs, (const mpz_t *) moduli, op->c+3, obf->rng);
        encode_Rc(obf->enc_vt, obf->op, obf->Rc, obf->sp, rs);
/* #pragma omp parallel for */
        for (size_t j = 0; j < op->m; j++) {
            encode_Zcj(obf->enc_vt, obf->op, obf->Zcj[j], obf->sp, obf->rng, rs,
                       ykj[op->c][j], op->circ->consts[j], (const mpz_t *) moduli);
        }
        mpz_vect_free(rs, op->c+3);
    }

    // encode Rhatkso and Zhatkso
/* #pragma omp parallel for schedule(dynamic,1) collapse(3) */
    for (size_t o = 0; o < op->gamma; o++) {
        for (size_t k = 0; k < op->c; k++) {
            for (size_t s = 0; s < op->q; s++) {
                mpz_t *tmp = mpz_vect_new(op->c+3);
                mpz_vect_urandomms(tmp, (const mpz_t *) moduli, op->c+3, obf->rng);
                encode_Rhatkso(obf->enc_vt, obf->op, obf->Rhatkso[k][s][o],
                               obf->sp, tmp, k, s, o);
                encode_Zhatkso(obf->enc_vt, obf->op, obf->Zhatkso[k][s][o],
                               obf->sp, (const mpz_t *) tmp, (const mpz_t *) whatk[k], k, s, o, (const mpz_t *) moduli);
                mpz_vect_free(tmp, op->c+3);
            }
        }
    }

    // encode Rhato and Zhato
/* #pragma omp parallel for */
    for (size_t o = 0; o < op->gamma; o++) {
        mpz_t *tmp = mpz_vect_new(op->c+3);
        mpz_vect_urandomms(tmp, (const mpz_t *) moduli, op->c+3, obf->rng);
        encode_Rhato(obf->enc_vt, op, obf->Rhato[o], obf->sp, tmp, o);
        encode_Zhato(obf->enc_vt, op, obf->Zhato[o], obf->sp, (const mpz_t *) tmp, (const mpz_t *) what, o,
                     (const mpz_t *) moduli);
        mpz_vect_free(tmp, op->c+3);
    }

    // encode Rbaro and Zbaro
/* #pragma omp parallel for */
    for (size_t o = 0; o < op->gamma; o++) {
        mpz_t *tmp = mpz_vect_new(op->c+3);
        mpz_vect_urandomms(tmp, (const mpz_t *) moduli, op->c+3, obf->rng);
        encode_Rbaro(obf->enc_vt, op, obf->Rbaro[o], obf->sp, tmp, o);
        encode_Zbaro(obf->enc_vt, op, obf->Zbaro[o], obf->sp, (const mpz_t *) tmp, (const mpz_t *) what, (const mpz_t **) whatk,
                     (const mpz_t **) ykj, op->circ, o, (const mpz_t *) moduli);
        mpz_vect_free(tmp, op->c+3);
    }

    // delete ykj
    for (size_t k = 0; k < op->c; k++) {
        for (size_t j = 0; j < op->ell; j++)
            mpz_clear(ykj[k][j]);
        free(ykj[k]);
    }
    for (size_t j = 0; j < op->m; j++)
        mpz_clear(ykj[op->c][j]);
    free(ykj[op->c]);
    free(ykj);

    // delete whatk and what
    for (size_t k = 0; k < op->c; k++) {
        mpz_vect_free(whatk[k], op->c+3);
    }
    free(whatk);
    mpz_vect_free(what, op->c+3);
    mpz_vect_free(moduli, obf->mmap->sk->nslots(obf->sp->sk));

    return OK;
}

static int
_obfuscation_fwrite(const obfuscation *const obf, FILE *const fp)
{
    const obf_params_t *const op = obf->op;

    public_params_fwrite(obf->pp_vt, obf->pp, fp);

    encoding_fwrite(obf->enc_vt, obf->Zstar, fp);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            encoding_fwrite(obf->enc_vt, obf->Rks[k][s], fp);
        }
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                encoding_fwrite(obf->enc_vt, obf->Zksj[k][s][j], fp);
            }
        }
    }

    encoding_fwrite(obf->enc_vt, obf->Rc, fp);
    for (size_t j = 0; j < op->m; j++) {
        encoding_fwrite(obf->enc_vt, obf->Zcj[j], fp);
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t o = 0; o < op->gamma; o++) {
                encoding_fwrite(obf->enc_vt, obf->Rhatkso[k][s][o], fp);
                encoding_fwrite(obf->enc_vt, obf->Zhatkso[k][s][o], fp);
            }
        }
    }

    for (size_t o = 0; o < op->gamma; o++) {
        encoding_fwrite(obf->enc_vt, obf->Rhato[o], fp);
        encoding_fwrite(obf->enc_vt, obf->Zhato[o], fp);
    }

    for (size_t o = 0; o < op->gamma; o++) {
        encoding_fwrite(obf->enc_vt, obf->Rbaro[o], fp);
        encoding_fwrite(obf->enc_vt, obf->Zbaro[o], fp);
    }

    return OK;
}

static obfuscation *
_obfuscation_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *const fp)
{
    obfuscation *obf;

    obf = my_calloc(1, sizeof(obfuscation));
    if (obf == NULL)
        return NULL;

    obf->mmap = mmap;
    obf->pp_vt = lin_get_pp_vtable(mmap);
    obf->sp_vt = lin_get_sp_vtable(mmap);
    obf->enc_vt = lin_get_encoding_vtable(mmap);
    obf->op = op;
    obf->sp = NULL;
    obf->pp = my_calloc(1, sizeof(public_params));
    (void) public_params_fread(obf->pp_vt, obf->pp, op, fp);

    obf->Zstar = my_calloc(1, sizeof(encoding));
    encoding_fread(obf->enc_vt, obf->Zstar, fp);

    obf->Rks = my_calloc(op->c, sizeof(encoding**));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rks[k] = my_calloc(op->q, sizeof(encoding*));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rks[k][s] = my_calloc(1, sizeof(encoding));
            encoding_fread(obf->enc_vt, obf->Rks[k][s], fp);
        }
    }

    obf->Zksj = my_calloc(op->c, sizeof(encoding***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Zksj[k] = my_calloc(op->q, sizeof(encoding**));
        for (size_t s = 0; s < op->q; s++) {
            obf->Zksj[k][s] = my_calloc(op->ell, sizeof(encoding*));
            for (size_t j = 0; j < op->ell; j++) {
                obf->Zksj[k][s][j] = my_calloc(1, sizeof(encoding));
                encoding_fread(obf->enc_vt, obf->Zksj[k][s][j], fp);
            }
        }
    }

    obf->Rc = my_calloc(1, sizeof(encoding));
    encoding_fread(obf->enc_vt, obf->Rc, fp);
    obf->Zcj = my_calloc(op->m, sizeof(encoding*));
    for (size_t j = 0; j < op->m; j++) {
        obf->Zcj[j] = my_calloc(1, sizeof(encoding));
        encoding_fread(obf->enc_vt, obf->Zcj[j], fp);
    }

    obf->Rhatkso = my_calloc(op->c, sizeof(encoding***));
    obf->Zhatkso = my_calloc(op->c, sizeof(encoding***));
    for (size_t k = 0; k < op->c; k++) {
        obf->Rhatkso[k] = my_calloc(op->q, sizeof(encoding **));
        obf->Zhatkso[k] = my_calloc(op->q, sizeof(encoding **));
        for (size_t s = 0; s < op->q; s++) {
            obf->Rhatkso[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            obf->Zhatkso[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            for (size_t o = 0; o < op->gamma; o++) {
                obf->Rhatkso[k][s][o] = my_calloc(1, sizeof(encoding));
                obf->Zhatkso[k][s][o] = my_calloc(1, sizeof(encoding));
                encoding_fread(obf->enc_vt, obf->Rhatkso[k][s][o], fp);
                encoding_fread(obf->enc_vt, obf->Zhatkso[k][s][o], fp);
            }
        }
    }

    obf->Rhato = my_calloc(op->gamma, sizeof(encoding*));
    obf->Zhato = my_calloc(op->gamma, sizeof(encoding*));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rhato[o] = my_calloc(1, sizeof(encoding));
        obf->Zhato[o] = my_calloc(1, sizeof(encoding));
        encoding_fread(obf->enc_vt, obf->Rhato[o], fp);
        encoding_fread(obf->enc_vt, obf->Zhato[o], fp);
    }

    obf->Rbaro = my_calloc(op->gamma, sizeof(encoding*));
    obf->Zbaro = my_calloc(op->gamma, sizeof(encoding*));
    for (size_t o = 0; o < op->gamma; o++) {
        obf->Rbaro[o] = my_calloc(1, sizeof(encoding));
        obf->Zbaro[o] = my_calloc(1, sizeof(encoding));
        encoding_fread(obf->enc_vt, obf->Rbaro[o], fp);
        encoding_fread(obf->enc_vt, obf->Zbaro[o], fp);
    }

    return obf;
}

typedef struct {
    encoding *r;
    encoding *z;
    bool my_r; // whether this wire "owns" r
    bool my_z; // whether this wire "owns" z
    size_t d;
} wire;

static void
wire_init(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
          wire *const rop, const public_params *const pp,
          bool init_r, bool init_z)
{
    if (init_r) {
        rop->r = encoding_new(vt, pp_vt, pp);
    }
    if (init_z) {
        rop->z = encoding_new(vt, pp_vt, pp);
    }
    rop->d = 0;
    rop->my_r = init_r;
    rop->my_z = init_z;
}

static void
wire_init_from_encodings(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
                         wire *const rop, const public_params *const pp,
                         encoding *const r, encoding *const z)
{
    const obf_params_t *const op = pp_vt->params(pp);
    const level *const lvl = vt->mmap_set(z);
    rop->r = r;
    rop->z = z;
    rop->my_r = false;
    rop->my_z = false;
    rop->d = lvl->mat[op->q][op->c+1];
}

static void
wire_clear(const encoding_vtable *const vt, wire *const rop)
{
    if (rop->my_r) {
        encoding_free(vt, rop->r);
    }
    if (rop->my_z) {
        encoding_free(vt, rop->z);
    }
}

static void
wire_copy(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
          wire *const rop, const wire *const source, const public_params *const pp)
{
    rop->r = encoding_new(vt, pp_vt, pp);
    rop->z = encoding_new(vt, pp_vt, pp);
    encoding_set(vt, rop->r, source->r);
    encoding_set(vt, rop->z, source->z);
    rop->my_r = true;
    rop->my_z = true;
    rop->d = source->d;
}

static int
wire_mul(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
         wire *const rop, const wire *const x, const wire *const y,
         const public_params *const pp)
{
    if (encoding_mul(vt, pp_vt, rop->r, x->r, y->r, pp) == ERR)
        return ERR;
    if (encoding_mul(vt, pp_vt, rop->z, x->z, y->z, pp) == ERR)
        return ERR;
    rop->d = x->d + y->d;
    return OK;
}


static int
wire_add(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
         wire *const rop, const wire *const x, const wire *const y,
         const obfuscation *const obf, const public_params *const pp)
{
    int ret = ERR;
    if (x->d > y->d) {
        if (wire_add(vt, pp_vt, rop, y, x, obf, pp) == ERR)
            return ERR;
        ret = OK;
    } else {
        encoding *zstar, *tmp;
        size_t d = y->d - x->d;

        if (encoding_mul(vt, pp_vt, rop->r, x->r, y->r, pp) == ERR)
            return ERR;

        tmp = encoding_new(vt, pp_vt, pp);

        if (d > 1) {
            /* Compute Zstar^d */
            zstar = encoding_new(vt, pp_vt, pp);
            if (encoding_mul(vt, pp_vt, zstar, obf->Zstar, obf->Zstar, pp) == ERR)
                goto cleanup;
            for (size_t j = 2; j < d; j++) {
                if (encoding_mul(vt, pp_vt, zstar, zstar, obf->Zstar, pp) == ERR)
                    goto cleanup;
            }
        } else {
            zstar = obf->Zstar;
        }

        if (encoding_mul(vt, pp_vt, rop->z, x->z, y->r, pp) == ERR)
            goto cleanup;
        if (d > 0)
            if (encoding_mul(vt, pp_vt, rop->z, rop->z, zstar, pp) == ERR)
                goto cleanup;
        if (encoding_mul(vt, pp_vt, tmp, y->z, x->r, pp) == ERR)
            goto cleanup;
        if (encoding_add(vt, pp_vt, rop->z, rop->z, tmp, pp) == ERR)
            goto cleanup;

        rop->d = y->d;

        ret = OK;
    cleanup:
        if (d > 1) {
            encoding_free(vt, zstar);
        }
        encoding_free(vt, tmp);
    }
    return ret;
}

static int
wire_sub(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
         wire *const rop, const wire *const x, const wire *const y,
         const obfuscation *const obf, const public_params *const pp)
{
    size_t d = abs((int) y->d - (int) x->d);
    encoding *zstar, *tmp;
    int ret = ERR;

    if (d > 1) {
        zstar = encoding_new(vt, pp_vt, pp);
        if (encoding_mul(vt, pp_vt, zstar, obf->Zstar, obf->Zstar, pp) == ERR)
            goto cleanup;
        for (size_t j = 2; j < d; j++)
            if (encoding_mul(vt, pp_vt, zstar, zstar, obf->Zstar, pp) == ERR)
                goto cleanup;
    } else {
        zstar = obf->Zstar;
    }

    tmp = encoding_new(vt, pp_vt, pp);

    if (x->d <= y->d) {
        if (encoding_mul(vt, pp_vt, rop->z, x->z, y->r, pp) == ERR)
            goto cleanup;
        if (d > 0)
            if (encoding_mul(vt, pp_vt, rop->z, rop->z, zstar, pp) == ERR)
                goto cleanup;
        if (encoding_mul(vt, pp_vt, tmp, y->z, x->r, pp) == ERR)
            goto cleanup;
        if (encoding_sub(vt, pp_vt, rop->z, rop->z, tmp, pp) == ERR)
            goto cleanup;
        rop->d = y->d;
    } else {
        if (encoding_mul(vt, pp_vt, rop->z, x->z, y->r, pp) == ERR)
            goto cleanup;
        if (encoding_mul(vt, pp_vt, tmp, y->z, x->r, pp) == ERR)
            goto cleanup;
        if (d > 0)
            if (encoding_mul(vt, pp_vt, tmp, tmp, zstar, pp) == ERR)
                goto cleanup;
        if (encoding_sub(vt, pp_vt, rop->z, rop->z, tmp, pp) == ERR)
            goto cleanup;
        rop->d = x->d;
    }
    if (encoding_mul(vt, pp_vt, rop->r, x->r, y->r, pp) == ERR)
        goto cleanup;

    ret = OK;
cleanup:
    if (d > 1)
        encoding_free(vt, zstar);
    encoding_free(vt, tmp);
    return ret;
}

static int
wire_constrained_add(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
                     wire *const rop, const wire *const x, const wire *const y,
                     const obfuscation *const obf, const public_params *const pp)
{
    if (x->d > y->d) {
        if (wire_constrained_add(vt, pp_vt, rop, y, x, obf, pp) == ERR)
            return ERR;
        return OK;
    } else {
        size_t d = y->d - x->d;
        encoding *zstar;

        if (d > 1) {
            zstar = encoding_new(vt, pp_vt, pp);
            encoding_mul(vt, pp_vt, zstar, obf->Zstar, obf->Zstar, pp);
            for (size_t j = 2; j < d; j++)
                encoding_mul(vt, pp_vt, zstar, zstar, obf->Zstar, pp);
        } else {
            zstar = obf->Zstar;
        }

        if (d > 0) {
            encoding_mul(vt, pp_vt, rop->z, x->z, zstar, pp);
            encoding_add(vt, pp_vt, rop->z, rop->z, y->z, pp);
        } else {
            encoding_add(vt, pp_vt, rop->z, x->z, y->z, pp);
        }
        rop->r = x->r;
        rop->d = y->d;

        if (d > 1)
            encoding_free(vt, zstar);
    }
    return OK;
}

static int
wire_constrained_sub(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
                     wire *const rop, const wire *const x, const wire *const y,
                     const obfuscation *const obf, const public_params *const pp)
{
    size_t d = abs((int) y->d - (int) x->d);
    encoding *zstar;
    if (d > 1) {
        zstar = encoding_new(vt, pp_vt, pp);
        encoding_mul(vt, pp_vt, zstar, obf->Zstar, obf->Zstar, pp);
        for (size_t j = 2; j < d; j++)
            encoding_mul(vt, pp_vt, zstar, zstar, obf->Zstar, pp);
    } else {
        zstar = obf->Zstar;
    }

    if (x->d <= y->d) {
        if (d > 0) {
            encoding_mul(vt, pp_vt, rop->z, x->z, zstar, pp);
            encoding_sub(vt, pp_vt, rop->z, rop->z, y->z, pp);
        } else {
            encoding_sub(vt, pp_vt, rop->z, x->z, y->z, pp);
        }
        rop->d = y->d;
    } else {
        // x->d > y->d && d > 0
        encoding *tmp;
        tmp = encoding_new(vt, pp_vt, pp);
        encoding_mul(vt, pp_vt, tmp, y->z, zstar, pp);
        encoding_sub(vt, pp_vt, rop->z, rop->z, tmp, pp);
        encoding_free(vt, tmp);
        rop->d = x->d;
    }
    rop->r = x->r;

    if (d > 1)
        encoding_free(vt, zstar);
    return OK;
}

static bool
wire_type_eq(const wire *const x, const wire *const y)
{
    (void) x; (void) y;
    if (!level_eq(info(x->r)->lvl, info(y->r)->lvl))
        return false;
    if (!level_eq_z(info(x->z)->lvl, info(y->z)->lvl))
        return false;
    return true;
}


// TODO: save zstar pows for reuse within the circuit

static int
_evaluate(int *rop, const int *inps, const obfuscation *const obf)
{
    /* const obf_params_t *op = obf->op; */
    public_params *const pp = obf->pp;
    acirc *const c = obf->op->circ;
    int ret = OK;

    // determine each assignment s \in \Sigma from the input bits
    int input_syms[obf->op->c];
    for (size_t i = 0; i < obf->op->c; i++) {
        input_syms[i] = 0;
        for (size_t j = 0; j < obf->op->ell; j++) {
            sym_id sym = { i, j };
            acircref inp_bit = obf->op->rchunker(sym, c->ninputs, obf->op->c);
            if (obf->op->rachel_input)
                input_syms[i] += inps[inp_bit] * j;
            else
                input_syms[i] += inps[inp_bit] << j;
        }
    }

    int *known = my_calloc(c->nrefs, sizeof(int));
    wire **cache = my_calloc(c->nrefs, sizeof(wire*));

    for (size_t o = 0; o < obf->op->circ->noutputs; o++) {
        acircref root = c->outrefs[o];
        acirc_topo_levels *topo = acirc_topological_levels(c, root);
        for (int lvl = 0; lvl < topo->nlevels; lvl++) {
            // #pragma omp parallel for
            for (int i = 0; i < topo->level_sizes[lvl]; i++) {
                acircref ref = topo->levels[lvl][i];
                if (known[ref])
                    continue;

                acirc_operation op = c->gates[ref].op;
                acircref *args = c->gates[ref].args;
                wire *w = my_calloc(1, sizeof(wire));
                if (op == OP_INPUT) {
                    size_t xid = args[0];
                    sym_id sym = obf->op->chunker(xid, c->ninputs, obf->op->c);
                    int k = sym.sym_number;
                    int j = sym.bit_number;
                    int s = input_syms[k];
                    wire_init_from_encodings(obf->enc_vt, obf->pp_vt, w, pp, obf->Rks[k][s], obf->Zksj[k][s][j]);
                } else if (op == OP_CONST) {
                    size_t yid = args[0];
                    wire_init_from_encodings(obf->enc_vt, obf->pp_vt, w, pp, obf->Rc, obf->Zcj[yid]);
                } else { // op is some kind of gate
                    wire *x = cache[args[0]];
                    wire *y = cache[args[1]];

                    if (op == OP_MUL) {
                        wire_init(obf->enc_vt, obf->pp_vt, w, pp, true, true);
                        if (wire_mul(obf->enc_vt, obf->pp_vt, w, x, y, pp) == ERR)
                            ret = ERR;
                    } else if (wire_type_eq(x, y)) {
                        wire_init(obf->enc_vt, obf->pp_vt, w, pp, false, true);
                        if (op == OP_ADD) {
                            if (wire_constrained_add(obf->enc_vt, obf->pp_vt, w, x, y, obf, pp) == ERR)
                                ret = ERR;
                        } else if (op == OP_SUB) {
                            if (wire_constrained_sub(obf->enc_vt, obf->pp_vt, w, x, y, obf, pp) == ERR)
                                ret = ERR;
                        }
                    } else {
                        wire_init(obf->enc_vt, obf->pp_vt, w, pp, true, true);
                        if (op == OP_ADD) {
                            if (wire_add(obf->enc_vt, obf->pp_vt, w, x, y, obf, pp) == ERR)
                                ret = ERR;
                        } else if (op == OP_SUB) {
                            if (wire_sub(obf->enc_vt, obf->pp_vt, w, x, y, obf, pp) == ERR)
                                ret = ERR;
                        }
                    }
                }
                if (ret == ERR)
                    break;

                known[ref] = true;
                cache[ref] = w;
            }
            if (ret == ERR)
                break;
        }
        acirc_topo_levels_destroy(topo);

        if (ret == ERR)
            goto cleanup;

        wire tmp[1]; // stack allocated pointers
        wire outwire[1];
        wire_copy(obf->enc_vt, obf->pp_vt, outwire, cache[root], pp);

        // input consistency
        for (size_t k = 0; k < obf->op->c; k++) {
            wire_init_from_encodings(obf->enc_vt, obf->pp_vt, tmp, pp,
                                     obf->Rhatkso[k][input_syms[k]][o],
                                     obf->Zhatkso[k][input_syms[k]][o]);
            wire_mul(obf->enc_vt, obf->pp_vt, outwire, outwire, tmp, pp);
        }

        // output consistency
        wire_init_from_encodings(obf->enc_vt, obf->pp_vt, tmp, pp,
                                 obf->Rhato[o], obf->Zhato[o]);
        wire_mul(obf->enc_vt, obf->pp_vt, outwire, outwire, tmp, pp);

        // authentication
        wire_init_from_encodings(obf->enc_vt, obf->pp_vt, tmp, pp,
                                 obf->Rbaro[o], obf->Zbaro[o]);
        wire_sub(obf->enc_vt, obf->pp_vt, outwire, outwire, tmp, obf, pp);
        wire_clear(obf->enc_vt, tmp);

        rop[o] = encoding_is_zero(obf->enc_vt, obf->pp_vt, outwire->z, pp);
        if (rop[o] == ERR) {
            ret = ERR;
        }

        wire_clear(obf->enc_vt, outwire);
    }

cleanup:
    for (size_t x = 0; x < c->nrefs; x++) {
        if (known[x]) {
            wire_clear(obf->enc_vt, cache[x]);
            free(cache[x]);
        }
    }
    free(cache);
    free(known);

    return ret;
}

obfuscator_vtable lin_obfuscator_vtable = {
    .new =  _obfuscation_new,
    .free = _obfuscation_free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _obfuscation_fwrite,
    .fread = _obfuscation_fread,
};
