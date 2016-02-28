#include "util.h"

#include <fcntl.h>
#include <gmp.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

int g_verbose;

// XXX: The use of /dev/urandom is not secure; however, the supercomputer we run
// on doesn't appear to have enough entropy, and blocks for long periods of
// time.  Thus, we use /dev/urandom instead.
#ifndef RANDFILE
#  define RANDFILE "/dev/urandom"
#endif

double current_time(void) {
    struct timeval t;
    (void) gettimeofday(&t, NULL);
    return (double) (t.tv_sec + (double) (t.tv_usec / 1000000.0));
}

int seed_rng(gmp_randstate_t *rng) {
    int file;
    if ((file = open(RANDFILE, O_RDONLY)) == -1) {
        (void) fprintf(stderr, "Error opening %s\n", RANDFILE);
        return 1;
    } else {
        unsigned long seed;
        if (read(file, &seed, sizeof seed) == -1) {
            (void) fprintf(stderr, "Error reading from %s\n", RANDFILE);
            (void) close(file);
            return 1;
        } else {
            if (g_verbose)
                (void) fprintf(stderr, "  Seed: %lu\n", seed);

            gmp_randinit_default(*rng);
            gmp_randseed_ui(*rng, seed);
        }
    }
    if (file != -1)
        (void) close(file);
    return 0;
}

int load_mpz_scalar(const char *fname, mpz_t x) {
    FILE *f;
    if ((f = fopen(fname, "r")) == NULL) {
        perror(fname);
        return 1;
    }
    (void) mpz_inp_raw(x, f);
    (void) fclose(f);
    return 0;
}

int save_mpz_scalar(const char *fname, const mpz_t x) {
    FILE *f;
    if ((f = fopen(fname, "w")) == NULL) {
        perror(fname);
        return 1;
    }
    if (mpz_out_raw(f, x) == 0) {
        (void) fclose(f);
        return 1;
    }
    (void) fclose(f);
    return 0;
}

int load_mpz_vector(const char *fname, mpz_t *m, const int len) {
    FILE *f;
    if ((f = fopen(fname, "r")) == NULL) {
        perror(fname);
        return 1;
    }
    for (int i = 0; i < len; ++i) {
        (void) mpz_inp_raw(m[i], f);
    }
    (void) fclose(f);
    return 0;
}

int save_mpz_vector(const char *fname, const mpz_t *m, const int len) {
    FILE *f;
    if ((f = fopen(fname, "w")) == NULL) {
        perror(fname);
        return 1;
    }
    for (int i = 0; i < len; ++i) {
        if (mpz_out_raw(f, m[i]) == 0) {
            (void) fclose(f);
            return 1;
        }
    }
    (void) fclose(f);
    return 0;
}

int max(int x, int y) {
    if (x >= y)
        return x;
    else
        return y;
}

void print_array(int *xs, size_t len) {
    if (len == 1){
        printf("[%d]", xs[0]);
        return;
    }
    for (int i = 0; i < len; i++) {
        if (i == 0) {
            printf("[%d,", xs[i]);
        } else if (i == len - 1) {
            printf("%d]", xs[i]);
        } else {
            printf("%d,", xs[i]);
        }
    }
}

bool in_array(int x, int *ys, size_t len) {
    for (int i = 0; i < len; i++) {
        if (x == ys[i])
            return true;
    }
    return false;
}

bool any_in_array(int *xs, int xlen, int *ys, size_t ylen) {
    for (int i = 0; i < xlen; i++) {
        if (in_array(xs[i], ys, ylen))
            return true;
    }
    return false;
}

void mpz_random_inv(mpz_t rop, gmp_randstate_t rng, mpz_t modulus) {
    mpz_t inv;
    mpz_init(inv);
    do {
        mpz_urandomm(rop, rng, modulus);
    } while (mpz_invert(inv, rop, modulus) == 0);
    mpz_clear(inv);
}
