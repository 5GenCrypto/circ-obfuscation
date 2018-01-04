#pragma once

#include <aesrand/aesrand.h>
#include <gmp.h>
#include <mmap/mmap.h>

#include <stdbool.h>

#define OK    0
#define ERR (-1)

#define PRIVATE __attribute__ ((visibility ("hidden")))

typedef enum debug_e {
    ERROR = 0,
    WARN = 1,
    DEBUG = 2,
    INFO = 3
} debug_e;
extern debug_e g_debug;
extern bool g_verbose;

enum mmap_e {
    MMAP_CLT,
    MMAP_DUMMY,
};
char * mmap_to_string(enum mmap_e mmap);
const mmap_vtable * mmap_to_mmap(enum mmap_e mmap);

#define LOG_ERROR (g_debug >= ERROR)
#define LOG_WARN  (g_debug >= WARN)
#define LOG_DEBUG (g_debug >= DEBUG)
#define LOG_INFO  (g_debug >= INFO)

double current_time(void);

int max(int, int);

void mpz_randomm_inv(mpz_t rop, aes_randstate_t rng, const mpz_t modulus);

mpz_t * mpz_vect_new(size_t n);
void mpz_vect_init(mpz_t *vec, size_t n);
mpz_t * mpz_vect_create_of_fmpz(fmpz_t *fvec, size_t n);
void mpz_vect_print(mpz_t *vec, size_t n);
void mpz_vect_clear(mpz_t *vec, size_t n);
void mpz_vect_free(mpz_t *vec, size_t n);

void mpz_vect_urandomm(mpz_t *vec, const mpz_t modulus, size_t n, aes_randstate_t rng);
void mpz_vect_urandomms(mpz_t *vec, const mpz_t *moduli, size_t n, aes_randstate_t rng);

void mpz_vect_mul(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, size_t n);
void mpz_vect_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *moduli, size_t n);
void mpz_vect_mul_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, const mpz_t *moduli, size_t n);

void mpz_vect_set(mpz_t *rop, const mpz_t *xs, size_t n);

void mpz_vect_repeat_ui(mpz_t *vec, size_t x, size_t n);

size_t bit(size_t x, size_t i);

void * my_calloc(size_t nmemb, size_t size);
void * my_malloc(size_t size);
void * my_realloc(void *ptr, size_t size);

int mpz_fread(mpz_t *x, FILE *fp);
int mpz_fwrite(mpz_t x, FILE *fp);
int int_fread(int *x, FILE *fp);
int int_fwrite(int x, FILE *fp);
int ulong_fread(unsigned long *x, FILE *fp);
int ulong_fwrite(unsigned long x, FILE *fp);
int size_t_fread(size_t *x, FILE *fp);
int size_t_fwrite(size_t x, FILE *fp);
int bool_fread(bool *x, FILE *fp);
int bool_fwrite(bool x, FILE *fp);

void array_add(int *rop, const int *xs, const int *ys, size_t n);
bool array_eq(const int *xs, const int *ys, size_t n);
int array_sum(const int *xs, size_t n);
size_t array_max(const size_t *xs, size_t n);
void array_printstring_rev(const long *xs, size_t n);


void print_progress(size_t cur, size_t total);

bool
print_test_output(size_t num, const long *inp, size_t ninputs, const long *expected,
                  const long *got, size_t noutputs, bool lin);

int
char_to_int(char c);
char
int_to_char(int i);
int
memory(unsigned long *size, unsigned long *resident);
