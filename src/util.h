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

size_t array_sum_ui (size_t *xs, size_t n);

bool in_array     (int x, int *ys, size_t len);
bool any_in_array (int *xs, int xlen, int *ys, size_t ylen);
bool array_eq     (int *xs, int *ys, size_t len);
bool array_eq_ui  (size_t *xs, size_t *ys, size_t len);

void mpz_random_inv(mpz_t rop, gmp_randstate_t rng, mpz_t modulus);

mpz_t* mpz_vect_create     (size_t n);
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

#define PUT_NEWLINE(fp) assert(fputc('\n', (fp)) != EOF);
#define GET_NEWLINE(fp) assert(fgetc(fp) == '\n');
#define PUT_SPACE(fp)   assert(fputc(' ', (fp)) != EOF);
#define GET_SPACE(fp)   assert(fgetc(fp) == ' ');

#endif
