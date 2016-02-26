#include "obfuscate.h"

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
    mpz_t *chat = eval_circ_mod(c, c->outref, alphas, betas, mmap->gs[1]);
    gmp_printf("check = %Zd\n", *chat);
}
