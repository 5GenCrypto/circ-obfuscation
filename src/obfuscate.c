#include "obfuscate.h"

#include "util.h"

#include <gmp.h>
#include <stdlib.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// boilerplate (yikes)

void obfuscation_init (obfuscation *obf, fake_params *p)
{
    obf->op = malloc(sizeof(obf_params));
    obf_params_init_set(obf->op, p->op);
    obf_params op = *(obf->op);

    obf->Zstar = malloc(sizeof(encoding));
    encoding_init(obf->Zstar, p);

    obf->Rsk = malloc((1 << op.q) * sizeof(encoding**));
    for (int s = 0; s < (1 << op.q); s++) {
        obf->Rsk[s] = malloc(op.q * sizeof(encoding*));
        for (int k = 0; k < op.c; k++) {
            obf->Rsk[s][k] = malloc(sizeof(encoding));
            encoding_init(obf->Rsk[s][k], p);
        }
    }

    obf->Zsjk = malloc((1 << op.q) * sizeof(encoding***));
    for (int s = 0; s < (1 << op.q); s++) {
        obf->Zsjk[s] = malloc(op.q * sizeof(encoding**));
        for (int j = 0; j < op.q; j++) {
            obf->Zsjk[s][j] = malloc(op.c * sizeof(encoding*));
            for (int k = 0; k < op.c; k++) {
                obf->Zsjk[s][j][k] = malloc(sizeof(encoding));
                encoding_init(obf->Zsjk[s][j][k], p);
            }
        }
    }

    obf->Rc = malloc(sizeof(encoding));
    encoding_init(obf->Rc, p);
    obf->Zjc = malloc(op.m * sizeof(encoding*));
    for (int j = 0; j < op.m; j++) {
        obf->Zjc[j] = malloc(sizeof(encoding));
        encoding_init(obf->Zjc[j], p);
    }

    obf->Rhatsok = malloc((1 << op.q) * sizeof(encoding***));
    obf->Zhatsok = malloc((1 << op.q) * sizeof(encoding***));
    for (int s = 0; s < (1 << op.q); s++) {
        obf->Rhatsok[s] = malloc(op.gamma * sizeof(encoding**));
        obf->Zhatsok[s] = malloc(op.gamma * sizeof(encoding**));
        for (int o = 0; o < op.gamma; o++) {
            obf->Rhatsok[s][o] = malloc(op.c * sizeof(encoding*));
            obf->Zhatsok[s][o] = malloc(op.c * sizeof(encoding*));
            for (int k = 0; k < op.c; k++) {
                obf->Rhatsok[s][o][k] = malloc(sizeof(encoding));
                obf->Zhatsok[s][o][k] = malloc(sizeof(encoding));
                encoding_init(obf->Rhatsok[s][o][k], p);
                encoding_init(obf->Zhatsok[s][o][k], p);
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

void obfuscation_clear (obfuscation *obf)
{
    obf_params op = *(obf->op);
    encoding_clear(obf->Zstar);
    free(obf->Zstar);

    for (int s = 0; s < (1 << op.q); s++) {
        for (int k = 0; k < op.c; k++) {
            encoding_clear(obf->Rsk[s][k]);
            free(obf->Rsk[s][k]);
        }
        free(obf->Rsk[s]);
    }
    free(obf->Rsk);

    for (int s = 0; s < (1 << op.q); s++) {
        for (int j = 0; j < op.q; j++) {
            for (int k = 0; k < op.c; k++) {
                encoding_clear(obf->Zsjk[s][j][k]);
                free(obf->Zsjk[s][j][k]);
            }
            free(obf->Zsjk[s][j]);
        }
        free(obf->Zsjk[s]);
    }
    free(obf->Zsjk);

    encoding_clear(obf->Rc);
    free(obf->Rc);

    for (int j = 0; j < op.m; j++) {
        encoding_clear(obf->Zjc[j]);
        free(obf->Zjc[j]);
    }
    free(obf->Zjc);

    for (int s = 0; s < (1 << op.q); s++) {
        for (int o = 0; o < op.gamma; o++) {
            for (int k = 0; k < op.c; k++) {
                encoding_clear(obf->Rhatsok[s][o][k]);
                encoding_clear(obf->Zhatsok[s][o][k]);
                free(obf->Rhatsok[s][o][k]);
                free(obf->Zhatsok[s][o][k]);
            }
            free(obf->Rhatsok[s][o]);
            free(obf->Zhatsok[s][o]);
        }
        free(obf->Rhatsok[s]);
        free(obf->Zhatsok[s]);
    }
    free(obf->Rhatsok);
    free(obf->Zhatsok);

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

////////////////////////////////////////////////////////////////////////////////
// obfuscator

void obfuscate (obfuscation *obf, fake_params *p, circuit *circ, gmp_randstate_t *rng)
{
    encode_Zstar(obf->Zstar, p, rng);
    /*mpz_t *rhostars = malloc(p->op->c+1 * sizeof(mpz_t));*/
    /*for (int i = 0; i < n; i++) {*/
        /*mpz_init(alphas[i]);*/
        /*mpz_random_inv(alphas[i], rng, mmap->gs[1]);*/
    /*}*/

    /*mpz_t *betas = malloc(m * sizeof(mpz_t));*/
    /*for (int i = 0; i < m; i++) {*/
        /*mpz_init(betas[i]);*/
        /*mpz_random_inv(betas[i], rng, mmap->gs[1]);*/
    /*}*/

    // evaluate check circuit
    /*mpz_t *chat = eval_circ_mod(c, c->outref, alphas, betas, mmap->gs[1]);*/
    /*gmp_printf("check = %Zd\n", *chat);*/
}

void encode_Zstar (encoding *enc, fake_params *p, gmp_randstate_t *rng)
{
    printf("c+3 = %lu\n", p->op->c+3);
    mpz_t *inps = malloc(p->op->c+3 * sizeof(mpz_t));
    mpz_init_set_ui(inps[0], 1);
    mpz_init_set_ui(inps[1], 1);
    for (size_t i = 2; i < p->op->c+3; i++) {
        printf("%lu\n", i);
        mpz_init(inps[i]);
        mpz_urandomm(inps[i], *rng, p->moduli[i]);
    }

    level *vstar = level_create_vstar(p->op);

    encode(enc, inps, p->op->c+3, vstar);

    level_clear(vstar);
    for (int i = 0; i < p->op->c+3; i++) {
        mpz_clear(inps[i]);
    }
    free(inps);
}
