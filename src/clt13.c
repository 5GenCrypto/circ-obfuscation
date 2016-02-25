#include "clt13.h"
#include "util.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int write_setup_params(const clt_state *s, const char *dir, long nu);

int clt_setup(
    clt_state *s,
    ulong kappa,
    ulong lambda,
    ulong nzs,
    const int *pows,
    const char *dir,
    int verbose
){
    ulong alpha, beta, eta, rho_f;
    mpz_t *ps, *zs;
    double start, end;

    /* Calculate CLT parameters */
    s->nzs = nzs;
    alpha  = lambda;
    beta   = lambda;
    s->rho = lambda;
    rho_f  = kappa * (s->rho + alpha + 2);
    eta    = rho_f + alpha + 2 * beta + lambda + 8;
    s->nu  = eta - beta - rho_f - lambda - 3;
    s->n   = (int) (eta * log2((float) lambda));

    if (verbose) {
        fprintf(stderr, "  Security Parameter: %ld\n", lambda);
        fprintf(stderr, "  Kappa: %ld\n", kappa);
        fprintf(stderr, "  Alpha: %ld\n", alpha);
        fprintf(stderr, "  Beta: %ld\n", beta);
        fprintf(stderr, "  Eta: %ld\n", eta);
        fprintf(stderr, "  Nu: %ld\n", s->nu);
        fprintf(stderr, "  Rho: %ld\n", s->rho);
        fprintf(stderr, "  Rho_f: %ld\n", rho_f);
        fprintf(stderr, "  N: %ld\n", s->n);
        fprintf(stderr, "  Number of Zs: %ld\n", s->nzs);
    }

    ps = (mpz_t *) malloc(sizeof(mpz_t) * s->n);
    s->gs = (mpz_t *) malloc(sizeof(mpz_t) * s->n);
    s->crt_coeffs = (mpz_t *) malloc(sizeof(mpz_t) * s->n);
    zs = (mpz_t *) malloc(sizeof(mpz_t) * s->nzs);
    s->zinvs = (mpz_t *) malloc(sizeof(mpz_t) * s->nzs);

    seed_rng(&s->rng);

    /* initialize gmp variables */
    mpz_init_set_ui(s->x0, 1);
    mpz_init_set_ui(s->pzt, 0);
    for (ulong i = 0; i < s->n; ++i) {
        mpz_init_set_ui(ps[i], 1);
        mpz_inits(s->gs[i], s->crt_coeffs[i], NULL);
    }
    for (ulong i = 0; i < s->nzs; ++i) {
        mpz_inits(zs[i], s->zinvs[i], NULL);
    }

    /* Generate p_i's and g_i's, as well as q = \prod p_i */
    start = current_time();
#pragma omp parallel for
    for (ulong i = 0; i < s->n; ++i) {
        mpz_t p_unif;
        mpz_init(p_unif);
        // XXX: the primes generated here aren't officially uniform
        mpz_urandomb(p_unif, s->rng, eta);
        mpz_nextprime(ps[i], p_unif);
        mpz_urandomb(p_unif, s->rng, alpha);
        mpz_nextprime(s->gs[i], p_unif);
        mpz_clear(p_unif);
    }
    for (ulong i = 0; i < s->n; ++i) {
        mpz_mul(s->x0, s->x0, ps[i]);
    }
    end = current_time();
    if (g_verbose)
        (void) fprintf(stderr, "  Generating p_i's and g_i's: %f\n",
                       end - start);

    /* Compute CRT coefficients */
    start = current_time();
    //
    // This step is needed for making encoding efficient.  Unfortunately, the
    // CRT coefficients take an enormous amount of memory to compute / store.
    //
#pragma omp parallel for
    for (ulong i = 0; i < s->n; ++i) {
        mpz_t q;
        mpz_init(q);
        mpz_tdiv_q(q, s->x0, ps[i]);
        mpz_invert(s->crt_coeffs[i], q, ps[i]);
        mpz_mul(s->crt_coeffs[i], s->crt_coeffs[i], q);
        mpz_mod(s->crt_coeffs[i], s->crt_coeffs[i], s->x0);
        mpz_clear(q);
    }
    end = current_time();
    if (g_verbose)
        (void) fprintf(stderr, "  Generating CRT coefficients: %f\n",
                       end - start);

    /* Compute z_i's */
    start = current_time();
#pragma omp parallel for
    for (ulong i = 0; i < s->nzs; ++i) {
        do {
            mpz_urandomm(zs[i], s->rng, s->x0);
        } while (mpz_invert(s->zinvs[i], zs[i], s->x0) == 0);
    }
    end = current_time();
    if (g_verbose)
        (void) fprintf(stderr, "  Generating z_i's: %f\n", end - start);

    /* Compute pzt */
    start = current_time();
    {
        mpz_t zk, tmp;
        mpz_init(tmp);
        mpz_init_set_ui(zk, 1);
        // compute z_1^t_1 ... z_k^t_k mod q
        for (ulong i = 0; i < s->nzs; ++i) {
            mpz_powm_ui(tmp, zs[i], pows[i], s->x0);
            mpz_mul(zk, zk, tmp);
            mpz_mod(zk, zk, s->x0);
        }
#pragma omp parallel for
        for (ulong i = 0; i < s->n; ++i) {
            mpz_t tmp, qpi, rnd;
            mpz_inits(tmp, qpi, rnd, NULL);
            // compute (((g_i)^{-1} mod p_i) * z^k mod p_i) * r_i * (q / p_i)
            mpz_invert(tmp, s->gs[i], ps[i]);
            mpz_mul(tmp, tmp, zk);
            mpz_mod(tmp, tmp, ps[i]);
            mpz_urandomb(rnd, s->rng, beta);
            mpz_mul(tmp, tmp, rnd);
            mpz_div(qpi, s->x0, ps[i]);
            mpz_mul(tmp, tmp, qpi);
            mpz_mod(tmp, tmp, s->x0);
#pragma omp critical
            {
                mpz_add(s->pzt, s->pzt, tmp);
            }
            mpz_clears(tmp, qpi, rnd, NULL);
        }
        mpz_mod(s->pzt, s->pzt, s->x0);
        mpz_clear(zk);
    }
    end = current_time();
    if (g_verbose)
        (void) fprintf(stderr, "  Generating pzt: %f\n", end - start);

    if (dir)
        (void) write_setup_params(s, dir, s->nu);

    for (ulong i = 0; i < s->n; ++i) {
        mpz_clear(ps[i]);
    }
    free(ps);
    for (ulong i = 0; i < s->nzs; ++i) {
        mpz_clear(zs[i]);
    }
    free(zs);

    return 0;
}

void clt_state_clear(clt_state *s) {
    gmp_randclear(s->rng);
    mpz_clears(s->x0, s->pzt, NULL);
    for (ulong i = 0; i < s->n; ++i) {
        mpz_clears(s->gs[i], s->crt_coeffs[i], NULL);
    }
    free(s->gs);
    free(s->crt_coeffs);
    for (ulong i = 0; i < s->nzs; ++i) {
        mpz_clear(s->zinvs[i]);
    }
    free(s->zinvs);
}

void clt_pp_init( clt_state *mmap, clt_public_parameters *pp ) {
    mpz_inits(pp->x0, pp->pzt, NULL);
    mpz_set(pp->x0, mmap->x0);
    mpz_set(pp->pzt, mmap->pzt);
    pp->nu = mmap->nu;
}

void clt_pp_clear( clt_public_parameters *pp ) {
    mpz_clears(pp->x0, pp->pzt, NULL);
    pp->nu = 0;
}

static int write_setup_params(const clt_state *s, const char *dir, long nu) {
    char *fname;
    int len;
    mpz_t tmp;
    double start, end;
    start = current_time();

    len = strlen(dir) + 10;

    fname = (char *) malloc(sizeof(char) * len);
    if (fname == NULL)
        return 1;

    mpz_init(tmp);

    // save nu
    mpz_set_ui(tmp, nu);
    (void) snprintf(fname, len, "%s/nu", dir);
    (void) save_mpz_scalar(fname, tmp);
    // save q
    (void) snprintf(fname, len, "%s/q", dir);
    (void) save_mpz_scalar(fname, s->x0);
    // save pzt
    (void) snprintf(fname, len, "%s/pzt", dir);
    (void) save_mpz_scalar(fname, s->pzt);

    mpz_clear(tmp);

    free(fname);

    end = current_time();
    if (g_verbose)
        (void) fprintf(stderr, "  Saving to file: %f\n", end - start);

    return 0;
}

void clt_encode( clt_state *s, mpz_t out, size_t nins, const mpz_t *ins, const int *pows) {
    mpz_t r, tmp;
    mpz_inits(r, tmp, NULL);
    mpz_set_ui(out, 0);
    for (ulong i = 0; i < s->n; i++) {
        mpz_urandomb(r, s->rng, s->rho);
        mpz_mul(tmp, r, s->gs[i]);
        if (i < nins) {
            mpz_add(tmp, tmp, ins[i]);
        }
        mpz_mul(tmp, tmp, s->crt_coeffs[i]);
        mpz_add(out, out, tmp);
    }
    for (size_t i = 0; i < s->nzs; i++) {
        if (pows[i] <= 0) continue;
        mpz_powm_ui(tmp, s->zinvs[i], pows[i], s->x0);
        mpz_mul(out, out, tmp);
        mpz_mod(out, out, s->x0);
    }
    mpz_clears(r, tmp, NULL);
}

int clt_is_zero( clt_public_parameters *pp, const mpz_t c) {
    int ret;

    mpz_t tmp, x0_;
    mpz_inits(tmp, x0_, NULL);

    mpz_mul(tmp, c, pp->pzt);
    mpz_mod(tmp, tmp, pp->x0);

    mpz_cdiv_q_ui(x0_, pp->x0, 2);
    if (mpz_cmp(tmp, x0_) > 0)
        mpz_sub(tmp, tmp, pp->x0);

    ret = mpz_sizeinbase(tmp, 2) < mpz_sizeinbase(pp->x0, 2) - pp->nu;
    mpz_clears(tmp, x0_, NULL);
    return ret;
}
