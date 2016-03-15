#include "obfuscate.h"

#include "util.h"

#include <gmp.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// boilerplate (yikes)

void obfuscation_init (obfuscation *obf, fake_params *p)
{
    obf->op = p->op;
    obf_params op = *(p->op);
    encoding_init(obf->Zstar, p);
    obf->Rsk  = malloc(op.c * sizeof(encoding**));
    obf->Zsjk = malloc(op.c * sizeof(encoding***));
    for (int k = 0; k < op.c; k++) {
        obf->Rsk[k]  = malloc((1 << op.q) * sizeof(encoding*));
        obf->Zsjk[k] = malloc((1 << op.q) * sizeof(encoding**));
        for (int s = 0; s < (1 << op.q); s++) {
            encoding_init(obf->Rsk[k][s], p);
            obf->Zsjk[k][s] = malloc(op.q * sizeof(encoding*));
            for (int j = 0; j < op.q; j++) {
                encoding_init(obf->Zsjk[k][s][j], p);
            }
        }
    }
    encoding_init(obf->Rc, p);
    obf->Zjc = malloc(op.m * sizeof(encoding*));
    for (int j = 0; j < op.m; j++) {
        encoding_init(obf->Zjc[j], p);
    }
    obf->Rhatsok = malloc(op.c * sizeof(encoding***));
    obf->Zhatsok = malloc(op.c * sizeof(encoding***));
    for (int k = 0; k < op.c; k++) {
        obf->Rhatsok[k] = malloc((1 << op.q) * sizeof(encoding**));
        obf->Zhatsok[k] = malloc((1 << op.q) * sizeof(encoding**));
        for (int s = 0; s < (1 << op.q); s++) {
            obf->Rhatsok[k][s] = malloc(op.gamma * sizeof(encoding*));
            obf->Zhatsok[k][s] = malloc(op.gamma * sizeof(encoding*));
            for (int o = 0; o < op.gamma; o++) {
                encoding_init(obf->Rhatsok[k][s][o], p);
                encoding_init(obf->Zhatsok[k][s][o], p);
            }
        }
    }
    obf->Rbaro = malloc(op.gamma * sizeof(encoding*));
    obf->Zbaro = malloc(op.gamma * sizeof(encoding*));
    for (int o = 0; o < op.gamma; o++) {
        encoding_init(obf->Rbaro[o], p);
        encoding_init(obf->Zbaro[o], p);
    }
}

void obfuscation_clear (obfuscation *obf)
{
    obf_params op = *(obf->op);
    encoding_clear(obf->Zstar);
    for (int k = 0; k < op.c; k++) {
        for (int s = 0; s < (1 << op.q); s++) {
            encoding_clear(obf->Rsk[k][s]);
            for (int j = 0; j < op.q; j++) {
                encoding_clear(obf->Zsjk[k][s][j]);
            }
            free(obf->Zsjk[k][s]);
        }
        free(obf->Rsk[k]);
        free(obf->Zsjk[k]);
    }
    free(obf->Rsk);
    free(obf->Zsjk);
    encoding_clear(obf->Rc);
    for (int j = 0; j < op.m; j++) {
        encoding_clear(obf->Zjc[j]);
    }
    free(obf->Zjc);
    for (int k = 0; k < op.c; k++) {
        for (int s = 0; s < (1 << op.q); s++) {
            for (int o = 0; o < op.gamma; o++) {
                encoding_clear(obf->Rhatsok[k][s][o]);
                encoding_clear(obf->Zhatsok[k][s][o]);
            }
            free(obf->Rhatsok[k][s]);
            free(obf->Zhatsok[k][s]);
        }
        free(obf->Rhatsok[k]);
        free(obf->Zhatsok[k]);
    }
    free(obf->Rhatsok);
    free(obf->Zhatsok);
    for (int o = 0; o < op.gamma; o++) {
        encoding_clear(obf->Rbaro[o]);
        encoding_clear(obf->Zbaro[o]);
    }
    free(obf->Rbaro);
    free(obf->Zbaro);
    obf_params_clear(obf->op);
}

////////////////////////////////////////////////////////////////////////////////
// obfuscator

void obfuscate (obfuscation *obf, fake_params *p, circuit *circ)
{
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
