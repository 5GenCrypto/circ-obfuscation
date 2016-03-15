#include "obfuscate.h"

#include "util.h"

#include <gmp.h>
#include <stdlib.h>

// for encodings parameterized by "s \in \Sigma", use the assignment as
// the index: s \in [2^q]
typedef struct {
    params *p;
    encoding *Zstar;
    encoding ***Rsk;        // k \in [c], s \in \Sigma
    encoding ****Zsjk;      // k \in [c], s \in \Sigma, j \in [\ell]
    encoding *Rc;
    encoding **Zjc;         // j \in [m] where m is length of secret P
    encoding ****Rhatsok;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding ****Zhatsok;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding **Rhato;       // o \in \Gamma
    encoding **Zhato;       // o \in \Gamma
    encoding **Rbaro;       // o \in \Gamma
    encoding **Zbaro;       // o \in \Gamma
} obfuscation;

void obfuscation_init  (obfuscation *obf, params *p)
{
    obf->p = p;

    encoding_init(obf->Zstar);

    obf->Rsk = malloc(p->c * sizeof(encoding**));
    for (int k = 0; k < p->c; k++) {
        obf->Rsk[k] = malloc(p->q * sizeof(encoding*));
        for (int s = 0; s < p->q; s++) {
            encoding_init
        }
    }
}


void obfuscation_clear (obfuscation *obf);



void obfuscate( clt_state *mmap, circuit *c ) {
    int n = c->ninputs;
    int m = c->nconsts;

    gmp_randstate_t rng;
    seed_rng(&rng);

    mpz_t *alphas = malloc(n * sizeof(mpz_t));
    for (int i = 0; i < n; i++) {
        mpz_init(alphas[i]);
        mpz_random_inv(alphas[i], rng, mmap->gs[1]);
    }

    mpz_t *betas = malloc(m * sizeof(mpz_t));
    for (int i = 0; i < m; i++) {
        mpz_init(betas[i]);
        mpz_random_inv(betas[i], rng, mmap->gs[1]);
    }

    // evaluate check circuit
    /*mpz_t *chat = eval_circ_mod(c, c->outref, alphas, betas, mmap->gs[1]);*/
    /*gmp_printf("check = %Zd\n", *chat);*/
}
