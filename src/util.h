#ifndef __IND_OBFUSCATION__UTILS_H__
#define __IND_OBFUSCATION__UTILS_H__

#include <stdbool.h>
#include <gmp.h>

extern int g_verbose;

double current_time(void);

int seed_rng(gmp_randstate_t *rng);

int load_mpz_scalar(const char *fname, mpz_t x);
int save_mpz_scalar(const char *fname, const mpz_t x);
int load_mpz_vector(const char *fname, mpz_t *m, const int len);
int save_mpz_vector(const char *fname, const mpz_t *m, const int len);

void mult_mats(mpz_t *result, const mpz_t *left, const mpz_t *right, const mpz_t q, long m, long n, long p);
void mult_vect_by_mat(mpz_t *v, const mpz_t *m, mpz_t q, int size, mpz_t *tmparray);
void mult_vect_by_vect(mpz_t out, const mpz_t *m, const mpz_t *v, mpz_t q, int size);

int max(int, int);

void print_array(int*, size_t);
bool in_array(int x, int *ys, size_t len);
bool any_in_array(int *xs, int xlen, int *ys, size_t ylen);

void mpz_random_inv(mpz_t rop, gmp_randstate_t rng, mpz_t modulus);

mpz_t* mpz_vect_create (size_t n);
void mpz_vect_destroy  (mpz_t *vec, size_t n);
void mpz_urandomm_vect (mpz_t *vec, mpz_t *moduli, size_t n, gmp_randstate_t *rng);
void mpz_vect_mul      (mpz_t *rop, mpz_t *xs, mpz_t *ys, size_t n);
void mpz_vect_mod      (mpz_t *rop, mpz_t *xs, mpz_t *moduli, size_t n);

size_t bit(size_t x, size_t i);

void* lin_calloc(size_t nmemb, size_t size);
void* lin_malloc(size_t size);
void* lin_realloc(void *ptr, size_t size);

#endif
