#include "obfuscate.h"

#include "util.h"
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// boilerplate

void obfuscation_init (obfuscation *obf, fake_params *p)/*{{{*/
{
    obf->op = malloc(sizeof(obf_params));
    obf_params_init_set(obf->op, p->op);
    obf_params op = *(obf->op);

    obf->Zstar = malloc(sizeof(encoding));
    encoding_init(obf->Zstar, p);

    obf->Rks = malloc(op.c * sizeof(encoding**));
    for (int k = 0; k < op.c; k++) {
        obf->Rks[k] = malloc(op.q * sizeof(encoding*));
        for (int s = 0; s < op.q; s++) {
            obf->Rks[k][s] = malloc(sizeof(encoding));
            encoding_init(obf->Rks[k][s], p);
        }
    }

    obf->Zksj = malloc(op.c * sizeof(encoding***));
    for (int k = 0; k < op.c; k++) {
        obf->Zksj[k] = malloc(op.q * sizeof(encoding**));
        for (int s = 0; s < op.q; s++) {
            obf->Zksj[k][s] = malloc(op.ell * sizeof(encoding*));
            for (int j = 0; j < op.ell; j++) {
                obf->Zksj[k][s][j] = malloc(sizeof(encoding));
                encoding_init(obf->Zksj[k][s][j], p);
            }
        }
    }

    obf->Rc = malloc(sizeof(encoding));
    encoding_init(obf->Rc, p);
    obf->Zcj = malloc(op.m * sizeof(encoding*));
    for (int j = 0; j < op.m; j++) {
        obf->Zcj[j] = malloc(sizeof(encoding));
        encoding_init(obf->Zcj[j], p);
    }

    obf->Rhatkso = malloc(op.c * sizeof(encoding***));
    obf->Zhatkso = malloc(op.c * sizeof(encoding***));
    for (int k = 0; k < op.c; k++) {
        obf->Rhatkso[k] = malloc(op.q * sizeof(encoding**));
        obf->Zhatkso[k] = malloc(op.q * sizeof(encoding**));
        for (int s = 0; s < op.q; s++) {
            obf->Rhatkso[k][s] = malloc(op.gamma * sizeof(encoding*));
            obf->Zhatkso[k][s] = malloc(op.gamma * sizeof(encoding*));
            for (int o = 0; o < op.gamma; o++) {
                obf->Rhatkso[k][s][o] = malloc(sizeof(encoding));
                obf->Zhatkso[k][s][o] = malloc(sizeof(encoding));
                encoding_init(obf->Rhatkso[k][s][o], p);
                encoding_init(obf->Zhatkso[k][s][o], p);
            }
        }
    }

    obf->Rbaro = malloc(op.gamma * sizeof(encoding*));
    obf->Zbaro = malloc(op.gamma * sizeof(encoding*));
    for (int o = 0; o < op.gamma; o++) {
        obf->Rbaro[o] = malloc(sizeof(encoding));
        obf->Zbaro[o] = malloc(sizeof(encoding));
        encoding_init(obf->Rbaro[o], p);
        encoding_init(obf->Zbaro[o], p);
    }
}
/*}}}*/
void obfuscation_clear (obfuscation *obf)/*{{{*/
{
    obf_params op = *(obf->op);

    encoding_clear(obf->Zstar);
    free(obf->Zstar);

    for (int k = 0; k < op.c; k++) {
        for (int s = 0; s < op.q; s++) {
            encoding_clear(obf->Rks[k][s]);
            free(obf->Rks[k][s]);
        }
        free(obf->Rks[k]);
    }
    free(obf->Rks);

    for (int k = 0; k < op.c; k++) {
        for (int s = 0; s < op.q; s++) {
            for (int j = 0; j < op.ell; j++) {
                encoding_clear(obf->Zksj[k][s][j]);
                free(obf->Zksj[k][s][j]);
            }
            free(obf->Zksj[k][s]);
        }
        free(obf->Zksj[k]);
    }
    free(obf->Zksj);

    encoding_clear(obf->Rc);
    free(obf->Rc);

    for (int j = 0; j < op.m; j++) {
        encoding_clear(obf->Zcj[j]);
        free(obf->Zcj[j]);
    }
    free(obf->Zcj);

    for (int k = 0; k < op.c; k++) {
        for (int s = 0; s < op.q; s++) {
            for (int o = 0; o < op.gamma; o++) {
                encoding_clear(obf->Rhatkso[k][s][o]);
                encoding_clear(obf->Zhatkso[k][s][o]);
                free(obf->Rhatkso[k][s][o]);
                free(obf->Zhatkso[k][s][o]);
            }
            free(obf->Rhatkso[k][s]);
            free(obf->Zhatkso[k][s]);
        }
        free(obf->Rhatkso[k]);
        free(obf->Zhatkso[k]);
    }
    free(obf->Rhatkso);
    free(obf->Zhatkso);

    for (int o = 0; o < op.gamma; o++) {
        encoding_clear(obf->Rbaro[o]);
        encoding_clear(obf->Zbaro[o]);
        free(obf->Rbaro[o]);
        free(obf->Zbaro[o]);
    }
    free(obf->Rbaro);
    free(obf->Zbaro);

    obf_params_clear(obf->op);
    free(obf->op);
}
/*}}}*/

////////////////////////////////////////////////////////////////////////////////
// obfuscator

void obfuscate (obfuscation *obf, fake_params *p, circuit *circ, gmp_randstate_t *rng)
{
    // create ykj
    mpz_t **ykj = malloc((p->op->c+1) * sizeof(mpz_t*));
    for (int k = 0; k < p->op->c; k++) {
        ykj[k] = malloc(p->op->ell * sizeof(mpz_t));
        for (int j = 0; j < p->op->ell; j++) {
            mpz_init(ykj[k][j]);
            mpz_urandomm(ykj[k][j], *rng, p->moduli[0]);
        }
    }
    // the cth ykj has length m (number of constant bits)
    ykj[p->op->c] = malloc(p->op->m * sizeof(mpz_t));
    for (int j = 0; j < p->op->m; j++) {
        mpz_init(ykj[p->op->c][j]);
        mpz_urandomm(ykj[p->op->c][j], *rng, p->moduli[0]);
    }

    // create wk
    mpz_t **whatk = malloc((p->op->c+1) * sizeof(mpz_t*));
    for (int k = 0; k < p->op->c+1; k++) {
        whatk[k] = mpz_vect_create(p->op->c+3);
        mpz_urandomm_vect(whatk[k], p->moduli, p->op->c+3, rng);
        mpz_set_ui(whatk[k][k+2], 0);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // obfuscation

    encode_Zstar(obf->Zstar, p, rng);

    // encode Rks and Zksj
    mpz_t *rs = mpz_vect_create(p->op->c+3);
    for (int k = 0; k < p->op->c; k++) {
        for (int s = 0; s < p->op->q; s++) {
            mpz_urandomm_vect(rs, p->moduli, p->op->c+3, rng);
            encode_Rks(obf->Rks[k][s], p, rs, k, s);
            for (int j = 0; j < p->op->ell; j++) {
                encode_Zksj(obf->Zksj[k][s][j], p, rs, ykj[k][j], k, s, j, rng);
            }
        }
    }

    // encode Rc and Zcj
    mpz_urandomm_vect(rs, p->moduli, p->op->c+3, rng);
    encode_Rc(obf->Rc, p, rs);
    for (int j = 0; j < p->op->m; j++) {
        encode_Zcj(obf->Zcj[j], p, rs, ykj[p->op->c][j], circ->consts[j], rng);
    }

    // encode Rhatkso and Zhatkso
    for (int o = 0; o < p->op->gamma; o++) {
        for (int k = 0; k < p->op->c; k++) {
            for (int s = 0; s < p->op->q; s++) {
                mpz_urandomm_vect(rs, p->moduli, p->op->c+3, rng);
                encode_Rhatkso(obf->Rhatkso[k][s][o], p, rs, k, s, o);
                encode_Zhatkso(obf->Zhatkso[k][s][o], p, rs, whatk[k], k, s, o);
            }
        }
    }

    // encode Rbaro and Zbaro
    for (int o = 0; o < p->op->gamma; o++) {
        mpz_urandomm_vect(rs, p->moduli, p->op->c+3, rng);
        encode_Rbaro(obf->Rbaro[o], p, rs, o);
        encode_Zbaro(obf->Zbaro[o], p, rs, whatk, ykj, circ, o);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // cleanup

    mpz_vect_destroy(rs, p->op->c+3);

    // delete ykj
    for (int k = 0; k < p->op->c; k++) {
        for (int j = 0; j < p->op->ell; j++)
            mpz_clear(ykj[k][j]);
        free(ykj[k]);
    }
    for (int j = 0; j < p->op->m; j++)
        mpz_clear(ykj[p->op->c][j]);
    free(ykj[p->op->c]);
    free(ykj);

    // delete whatk
    for (int k = 0; k < p->op->c+1; k++) {
        mpz_vect_destroy(whatk[k], p->op->c+3);
    }
    free(whatk);
}

void encode_Zstar (encoding *enc, fake_params *p, gmp_randstate_t *rng)
{
    mpz_t *inps = mpz_vect_create(p->op->c+3);
    mpz_set_ui(inps[0], 1);
    mpz_set_ui(inps[1], 1);
    mpz_urandomm_vect(inps + 2, p->moduli + 2, p->op->c+1, rng);

    level *vstar = level_create_vstar(p->op);
    encode(enc, inps, p->op->c+3, vstar);

    mpz_vect_destroy(inps, p->op->c+3);
    level_destroy(vstar);
}

void encode_Rks (encoding *enc, fake_params *p, mpz_t *rs, size_t k, size_t s)
{
    level *vsk = level_create_vks(p->op, k, s);
    encode(enc, rs, p->op->c+3, vsk);
    level_destroy(vsk);
}

void encode_Zksj (
    encoding *enc,
    fake_params *p,
    mpz_t *rs,
    mpz_t ykj,
    size_t k,
    size_t s,
    size_t j,
    gmp_randstate_t *rng
) {
    mpz_t *w = mpz_vect_create(p->op->c+3);
    mpz_urandomm_vect(w+2, p->moduli+2, p->op->c+1, rng);

    mpz_mul(w[0], rs[0], ykj);
    mpz_mod(w[0], w[0], p->moduli[0]);

    mpz_mul_ui(w[1], rs[1], bit(s, j));
    mpz_mod(w[1], w[1], p->moduli[1]);

    for (int i = 2; i < p->op->c+3; i++) {
        mpz_mul(w[i], w[i], rs[i]);
        mpz_mod(w[i], w[i], p->moduli[i]);
    }

    level *lvl = level_create_vks(p->op, k, s);
    level *vstar = level_create_vstar(p->op);
    level_add(lvl, lvl, vstar);

    encode(enc, w, p->op->c+3, lvl);

    level_destroy(vstar);
    level_destroy(lvl);
    mpz_vect_destroy(w, p->op->c+3);
}

void encode_Rc (encoding *enc, fake_params *p, mpz_t *rs)
{
    level *vc = level_create_vc(p->op);
    encode(enc, rs, p->op->c+3, vc);
    level_destroy(vc);
}

void encode_Zcj(encoding *enc, fake_params *p, mpz_t *rs, mpz_t ykj, int const_val, gmp_randstate_t *rng)
{
    mpz_t *w = mpz_vect_create(p->op->c+3);
    mpz_urandomm_vect(w+2, p->moduli+2, p->op->c+1, rng);

    mpz_mul(w[0], ykj, rs[0]);
    mpz_mod(w[0], w[0], p->moduli[0]);

    mpz_mul_ui(w[1], rs[1], const_val);
    mpz_mod(w[1], w[1], p->moduli[1]);

    for (int i = 2; i < p->op->c+3; i++) {
        mpz_mul(w[i], w[i], rs[i]);
        mpz_mod(w[i], w[i], p->moduli[i]);
    }

    level *lvl = level_create_vc(p->op);
    level *vstar = level_create_vstar(p->op);
    level_add(lvl, lvl, vstar);

    encode(enc, w, p->op->c+3, lvl);

    level_destroy(vstar);
    level_destroy(lvl);
    mpz_vect_destroy(w, p->op->c+3);
}

void encode_Rhatkso(encoding *enc, fake_params *p, mpz_t *rs, size_t k, size_t s, size_t o)
{
    level *vhatkso = level_create_vhatkso(p->op, k, s, o);
    encode(enc, rs, p->op->c+3, vhatkso);
    level_destroy(vhatkso);
}

void encode_Zhatkso(encoding *enc, fake_params *p, mpz_t *rs, mpz_t *w, size_t k, size_t s, size_t o)
{
    mpz_t *inp = mpz_vect_create(p->op->c+3);
    for (int i = 0; i < p->op->c+3; i++) {
        mpz_mul(inp[i], w[i], rs[i]);
        mpz_mod(inp[i], w[i], p->moduli[i]);
    }

    level *lvl   = level_create_vhatkso(p->op, k, s, o);
    level *vstar = level_create_vstar(p->op);
    level_add(lvl, lvl, vstar);

    encode(enc, inp, p->op->c+3, lvl);

    level_destroy(vstar);
    level_destroy(lvl);
    mpz_vect_destroy(inp, p->op->c+3);
}

void encode_Rbaro(encoding *enc, fake_params *p, mpz_t *rs, size_t o)
{
    level *vbaro = level_create_vbaro(p->op, o);
    encode(enc, rs, p->op->c+3, vbaro);
    level_destroy(vbaro);
}

void encode_Zbaro(encoding *enc, fake_params *p, mpz_t *rs, mpz_t **whatk, mpz_t **ykj, circuit *c, size_t o)
{

    mpz_t ybar;
    mpz_init(ybar);

    mpz_t *xs = mpz_vect_create(c->ninputs);
    for (int k = 0; k < p->op->c; k++) {
        for (int j = 0; j < p->op->ell; j++) {
            sym_id sym = {k, j};
            input_id id = p->op->rchunker(sym, c->ninputs, p->op->c);
            if (id >= c->ninputs)
                continue;
            mpz_set(xs[id], ykj[k][j]);
        }
    }
    eval_circ_mod(ybar, c, c->outrefs[o], xs, ykj[p->op->c], p->moduli[0]);

    mpz_t *tmp = mpz_vect_create(p->op->c+3);
    for (int i = 0; i < p->op->c+3; i++) {
        mpz_set_ui(tmp[i], 1);
    }
    for (int k = 0; k < p->op->c+1; k++) {
        mpz_vect_mul(tmp, whatk[k], tmp, p->op->c+3);
    }
    mpz_t *w = mpz_vect_create(p->op->c+3);
    mpz_set(w[0], ybar);
    mpz_set_ui(w[1], 1);
    mpz_vect_mul(w, w, tmp,       p->op->c+3);
    mpz_vect_mod(w, w, p->moduli, p->op->c+3);

    for (int i = 0; i < p->op->c+3; i++) {
        mpz_mul(w[i], w[i], rs[i]);
        mpz_mod(w[i], w[i], p->moduli[i]);
    }

    uint32_t d = degree(c, c->outrefs[o]);

    level *lvl   = level_create_vbaro(p->op, o);
    level *vstar = level_create_vstar(p->op);
    level_mul_ui(vstar, vstar, d + p->op->c + 1);
    level_add(lvl, lvl, vstar);

    encode(enc, w, p->op->c+3, lvl);

    mpz_clear(ybar);
    mpz_vect_destroy(tmp, p->op->c+3);
    mpz_vect_destroy(xs, c->ninputs);
    mpz_vect_destroy(w, p->op->c+3);
    level_destroy(vstar);
    level_destroy(lvl);
}
