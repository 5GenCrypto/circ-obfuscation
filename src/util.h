#pragma once

#include <aesrand.h>
#include <gmp.h>
#include <mmap/mmap.h>

#include <stdbool.h>

#define OK    0
#define ERR (-1)

#define PRIVATE __attribute__ ((visibility ("hidden")))

extern const char *errorstr;
extern const char *warnstr;
extern bool g_verbose;

double current_time(void);

static inline int
max(int a, int b) {
    return a > b ? a : b;
}
static inline int
min(int a, int b) {
    return a > b ? b : a;
}

void mpz_randomm_inv(mpz_t rop, aes_randstate_t rng, const mpz_t modulus);
mpz_t * mpz_vect_new(size_t n);
void mpz_vect_print(mpz_t *vec, size_t n);
void mpz_vect_free(mpz_t *vec, size_t n);
void mpz_vect_mul(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, size_t n);
void mpz_vect_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *moduli, size_t n);
void mpz_vect_mul_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, const mpz_t *moduli, size_t n);
void mpz_vect_set(mpz_t *rop, const mpz_t *xs, size_t n);
void mpz_vect_repeat_ui(mpz_t *vec, size_t x, size_t n);

size_t bit(size_t x, size_t i);

void * my_calloc(size_t nmemb, size_t size);

char * str_fread(FILE *fp);
int    str_fwrite(const char *x, FILE *fp);
int mpz_fread(mpz_t *x, FILE *fp);
int mpz_fwrite(mpz_t x, FILE *fp);
int int_fread(int *x, FILE *fp);
int int_fwrite(int x, FILE *fp);
int size_t_fread(size_t *x, FILE *fp);
int size_t_fwrite(size_t x, FILE *fp);
int bool_fread(bool *x, FILE *fp);
int bool_fwrite(bool x, FILE *fp);

void print_progress(size_t cur, size_t total);

bool print_test_output(size_t num, const long *inp, size_t ninputs, const long *expected,
                       const long *got, size_t noutputs, bool lin);

long char_to_long(char c);
char long_to_char(long i);
/* return memory usage (in megabytes) */
int memory(unsigned long *size, unsigned long *resident);
/* file size (in bytes) */
size_t filesize(const char *fname);

char * makestr(const char *format, ...);
int makedir(const char *dirname);

char * longs_to_str(const long *xs, size_t n);
long * str_to_longs(const char *str, size_t n);
