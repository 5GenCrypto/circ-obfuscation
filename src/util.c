#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>


#include <gmp.h>
#include <mmap/mmap_clt.h>
#include <mmap/mmap_dummy.h>

bool g_verbose = false;
debug_e g_debug = ERROR;

double current_time(void) {
    struct timeval t;
    (void) gettimeofday(&t, NULL);
    return (double) (t.tv_sec + (double) (t.tv_usec / 1000000.0));
}

int max(int x, int y) {
    if (x >= y)
        return x;
    else
        return y;
}

int array_sum(const int *xs, size_t n)
{
    size_t res = 0;
    for (size_t i = 0; i < n; ++i) {
        res += xs[i];
    }
    return res;
}

size_t array_max(const size_t *xs, size_t n)
{
    size_t max = 0;
    for (size_t i = 0; i < n; ++i) {
        if (max < xs[i])
            max = xs[i];
    }
    return max;
}

static void
array_printstring(const long *xs, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        printf("%c", long_to_char(xs[i]));
}

mpz_t *
mpz_vect_new(size_t n)
{
    mpz_t *xs = my_calloc(n, sizeof xs[0]);
    mpz_vect_init(xs, n);
    return xs;
}

void
mpz_vect_init(mpz_t *xs, size_t n)
{
    for (size_t i = 0; i < n; i++)
        mpz_init(xs[i]);
}

mpz_t * mpz_vect_create_of_fmpz(fmpz_t *fvec, size_t n)
{
    mpz_t *vec = mpz_vect_new(n);
    for (size_t i = 0; i < n; ++i) {
        fmpz_get_mpz(vec[i], fvec[i]);
        fmpz_clear(fvec[i]);
    }
    free(fvec);
    return vec;
}

void mpz_vect_print(mpz_t *xs, size_t len)
{
    if (len == 1){
        gmp_printf("[%Zd]", xs[0]);
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (i == 0) {
            gmp_printf("[%Zd,", xs[i]);
        } else if (i == len - 1) {
            gmp_printf("%Zd]", xs[i]);
        } else {
            gmp_printf("%Zd,", xs[i]);
        }
    }
}

void
mpz_vect_free(mpz_t *xs, size_t n)
{
    mpz_vect_clear(xs, n);
    free(xs);
}

void
mpz_vect_clear(mpz_t *xs, size_t n)
{
    for (size_t i = 0; i < n; i++)
        mpz_clear(xs[i]);
}

void mpz_vect_set(mpz_t *rop, const mpz_t *xs, size_t n)
{
    for (size_t i = 0; i < n; i++)
        mpz_set(rop[i], xs[i]);
}

void mpz_vect_urandomm(mpz_t *vec, const mpz_t modulus, size_t n, aes_randstate_t rng)
{
    for (size_t i = 0; i < n; i++) {
        do {
            mpz_urandomm_aes(vec[i], rng, modulus);
        } while (mpz_cmp_ui(vec[i], 0) == 0);
    }
}

void mpz_vect_urandomms(mpz_t *vec, const mpz_t *moduli, size_t n, aes_randstate_t rng)
{
    for (size_t i = 0; i < n; i++) {
        do {
            mpz_urandomm_aes(vec[i], rng, moduli[i]);
        } while (mpz_cmp_ui(vec[i], 0) == 0);
    }
}

void mpz_vect_mul(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        mpz_mul(rop[i], xs[i], ys[i]);
    }
}

void mpz_vect_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *moduli, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        mpz_mod(rop[i], xs[i], moduli[i]);
    }
}

void mpz_vect_mul_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, const mpz_t *moduli, size_t n)
{
    mpz_vect_mul(rop, xs, ys, n);
    mpz_vect_mod(rop, (const mpz_t *const) rop, moduli, n);
}

void
mpz_randomm_inv(mpz_t rop, aes_randstate_t rng, const mpz_t modulus)
{
    mpz_t inv;
    mpz_init(inv);
    do {
        mpz_urandomm_aes(rop, rng, modulus);
    } while (mpz_invert(inv, rop, modulus) == 0);
    mpz_clear(inv);
}

size_t bit(size_t x, size_t i)
{
    return (x & (1 << i)) > 0;
}

////////////////////////////////////////////////////////////////////////////////
// custom allocators that complain when they fail

void *
my_calloc(size_t nmemb, size_t size)
{
    void *ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        fprintf(stderr, "[%s] couldn't allocate %lu bytes!\n", __func__,
                nmemb * size);
        return NULL;
    }
    return ptr;
}

////////////////////////////////////////////////////////////////////////////////
// serialization

int
mpz_fread(mpz_t *x, FILE *fp)
{
    if (mpz_inp_raw(*x, fp) == 0) {
        fprintf(stderr, "error: reading mpz failed\n");
        return ERR;
    }
    (void) fscanf(fp, "\n");
    return OK;
}

int
mpz_fwrite(mpz_t x, FILE *fp)
{
    if (mpz_out_raw(fp, x) == 0) {
        fprintf(stderr, "error: writing mpz failed\n");
        return ERR;
    }
    (void) fprintf(fp, "\n");
    return OK;
}

int
int_fread(int *x, FILE *fp)
{
    if (fread(x, sizeof x[0], 1, fp) != 1) {
        fprintf(stderr, "error: reading int failed\n");
        return ERR;
    }
    return OK;
}

int
int_fwrite(int x, FILE *fp)
{
    if (fwrite(&x, sizeof x, 1, fp) != 1) {
        fprintf(stderr, "error: writing int failed\n");
        return ERR;
    }
    return OK;
}

int
ulong_fread(unsigned long *x, FILE *fp)
{
    if (fread(x, sizeof x[0], 1, fp) != 1) {
        fprintf(stderr, "error: reading unsigned long failed\n");
        return ERR;
    }
    return OK;
}

int
ulong_fwrite(unsigned long x, FILE *fp)
{
    if (fwrite(&x, sizeof x, 1, fp) != 1) {
        fprintf(stderr, "error: writing unsigned long failed\n");
        return ERR;
    }
    return OK;
}

int
size_t_fread(size_t *x, FILE *fp)
{
    if (fread(x, sizeof x[0], 1, fp) != 1) {
        fprintf(stderr, "error: reading size_t failed\n");
        return ERR;
    }
    return OK;
}

int
size_t_fwrite(size_t x, FILE *fp)
{
    if (fwrite(&x, sizeof x, 1, fp) != 1) {
        fprintf(stderr, "error: writing size_t failed\n");
        return ERR;
    }
    return OK;
}

int
bool_fread(bool *x, FILE *fp)
{
    if (fread(x, sizeof x[0], 1, fp) != 1) {
        fprintf(stderr, "error: reading bool failed\n");
        return ERR;
    }
    return OK;
}

int
bool_fwrite(bool x, FILE *fp)
{
    if (fwrite(&x, sizeof x, 1, fp) != 1) {
        fprintf(stderr, "error: writing bool failed\n");
        return ERR;
    }
    return OK;
}

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60

void
print_progress(size_t cur, size_t total)
{
    static int last_val = 0;
    double percentage = (double) cur / total;
    int val  = percentage * 100;
    int lpad = percentage * PBWIDTH;
    int rpad = PBWIDTH - lpad;
    if (val != last_val) {
        fprintf(stdout, "\r\t%3d%% [%.*s%*s] %lu/%lu", val, lpad, PBSTR, rpad, "", cur, total);
        if (cur == total)
            fprintf(stdout, "\n");
        fflush(stdout);
        last_val = val;
    }
}

char *
mmap_to_string(enum mmap_e mmap)
{
    switch (mmap) {
    case MMAP_CLT:
        return "CLT";
    case MMAP_DUMMY:
        return "DUMMY";
    }
    abort();
}

const mmap_vtable *
mmap_to_mmap(enum mmap_e mmap)
{
    switch (mmap) {
    case MMAP_CLT:
        return &clt_vtable;
    case MMAP_DUMMY:
        return &dummy_vtable;
    default:
        return NULL;
    }
}

bool
print_test_output(size_t num, const long *inp, size_t ninputs, const long *expected,
                  const long *got, size_t noutputs, bool lin)
{
    bool ok = true;
    for (size_t i = 0; i < noutputs; ++i) {
        if (lin)
            ok = !(got[i] == (expected[i] != 1));
        else
            ok = !(!!got[i] != !!expected[i]);
        if (!ok)
            break;
    }
    if (ok)
        printf("\033[1;42m");
    else
        printf("\033[1;41m");
    printf("Test #%lu: input=", num);
    array_printstring(inp, ninputs);
    if (ok)
        printf(" ✓ ");
    else
        printf(" ✗ ");
    printf("expected=");
    array_printstring(expected, noutputs);
    printf(" got=");
    array_printstring(got, noutputs);
    printf("\033[0m\n");
    return ok;
}

long
char_to_long(char c)
{
    if (toupper(c) >= 'A' && toupper(c) <= 'Z')
        return toupper(c) - 'A' + 10;
    else if (c >= '0' && c <= '9')
        return c - '0';
    else {
        fprintf(stderr, "error: invalid input '%c'\n", c);
        return -1;
    }
}

char
long_to_char(long i)
{
    if (i > 36)
        return '-';
    else if (i >= 10)
        return i + 55;
    else
        return i + 48;
}

int
memory(unsigned long *size, unsigned long *resident)
{
    FILE *fp;
    const char *f = "/proc/self/statm";

    if ((fp = fopen(f, "r")) == NULL)
        return ERR;
    if (fscanf(fp, "%ld %ld", size, resident) != 2) {
        return ERR;
    }
    fclose(fp);
    *size = *size * 4 / 1024;
    *resident = *resident * 4 / 1024;
    return OK;
}

size_t
filesize(const char *fname)
{
    struct stat st;
    if (stat(fname, &st) == 0)
        return st.st_size;
    else
        return 0;
}
