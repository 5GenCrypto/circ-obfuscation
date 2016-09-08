#ifndef __IND_OBFUSCATION__UTILS_H__
#define __IND_OBFUSCATION__UTILS_H__

#include "aesrand.h"

#include <stdbool.h>
#include <gmp.h>

extern int g_verbose;

double current_time(void);

int seed_rng(gmp_randstate_t *rng);

int max(int, int);

void array_print           (int*, size_t);
void array_print_ui        (size_t*, size_t);
void array_printstring     (int *bits, size_t n);
void array_printstring_rev (int *bits, size_t n);

void mpz_random_inv(mpz_t rop, gmp_randstate_t rng, mpz_t modulus);

mpz_t* mpz_vect_create     (size_t n);
mpz_t* mpz_vect_create_of_fmpz (fmpz_t *fvec, size_t n);
void mpz_vect_print        (mpz_t*, size_t);
void mpz_vect_destroy      (mpz_t *vec, size_t n);
void mpz_urandomm_vect     (mpz_t *vec, mpz_t *moduli, size_t n, gmp_randstate_t *rng);
void mpz_urandomm_vect_aes (mpz_t *vec, mpz_t *moduli, size_t n, aes_randstate_t rng);

void mpz_vect_mul (mpz_t *rop, mpz_t *xs, mpz_t *ys, size_t n);
void mpz_vect_mod (mpz_t *rop, mpz_t *xs, mpz_t *moduli, size_t n);
void mpz_vect_set (mpz_t *rop, mpz_t *xs, size_t n);

void mpz_vect_repeat_ui (mpz_t *vec, size_t x, size_t n);

size_t bit(size_t x, size_t i);

void* lin_calloc(size_t nmemb, size_t size);
void* lin_malloc(size_t size);
void* lin_realloc(void *ptr, size_t size);

void ulong_read (unsigned long *x, FILE *const fp);
void ulong_write (FILE *const fp, unsigned long x);

#define PUT_NEWLINE(fp) (!(fprintf(fp, "\n") > 0))
#define GET_NEWLINE(fp) fscanf(fp, "\n")
#define PUT_SPACE(fp) (!(fprintf(fp, " ") > 0))
#define GET_SPACE(fp) fscanf(fp, " ")

#define ARRAY_SUM(XS, N) ({     \
    size_t RES = 0;             \
    size_t I;                   \
    for (I = 0; I < N; I++) {   \
        RES += XS[I];           \
    }                           \
    RES;                        \
})

#define IN_ARRAY(ELEM, XS, N) ({    \
    size_t I;                       \
    bool RES = false;               \
    for (I = 0; I < N; I++) {       \
        if (ELEM == XS[I]) {        \
            RES = true;             \
            break;                  \
        }                           \
    }                               \
    RES;                            \
}

#define ANY_IN_ARRAY(XS, XLEN, YS, YLEN) ({ \
    size_t I;                               \
    bool RES = false;                       \
    for (I = 0; I < XLEN; I++) {            \
        if (in_array(XS[I], YS, YLEN)) {    \
            RES = true;                     \
            break;                          \
        }                                   \
    }                                       \
    RES;                                    \
})

#define ARRAY_EQ(XS, YS, N) ({   \
    size_t I;                   \
    bool RES = true;            \
    for (I = 0; I < N; I++)     \
        if (XS[I] != YS[I]) {   \
            RES = false;        \
            break;              \
        }                       \
    RES;                        \
})

#endif
