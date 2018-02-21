#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>


#include <gmp.h>
#include <mmap/mmap_clt.h>
#include <mmap/mmap_dummy.h>

const char *errorstr = "\033[1;31merror\033[0m";
const char *fatalstr = "\033[1;31mfatal\033[0m";
const char *warnstr = "\033[1;33mwarning\033[0m";

bool g_verbose = false;

double
current_time(void) {
    struct timeval t;
    (void) gettimeofday(&t, NULL);
    return (double) (t.tv_sec + (double) (t.tv_usec / 1000000.0));
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
    mpz_t *xs;
    xs = xcalloc(n, sizeof xs[0]);
    for (size_t i = 0; i < n; i++)
        mpz_init(xs[i]);
    return xs;
}

void
mpz_vect_print(mpz_t *xs, size_t len)
{
    if (len == 1) {
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
    for (size_t i = 0; i < n; i++)
        mpz_clear(xs[i]);
    free(xs);
}

void
mpz_vect_set(mpz_t *rop, const mpz_t *xs, size_t n)
{
    for (size_t i = 0; i < n; i++)
        mpz_set(rop[i], xs[i]);
}

void
mpz_vect_mul(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        mpz_mul(rop[i], xs[i], ys[i]);
    }
}

void
mpz_vect_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *moduli, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        mpz_mod(rop[i], xs[i], moduli[i]);
    }
}

void
mpz_vect_mul_mod(mpz_t *rop, const mpz_t *xs, const mpz_t *ys, const mpz_t *moduli, size_t n)
{
    mpz_vect_mul(rop, xs, ys, n);
    mpz_vect_mod(rop, (const mpz_t *) rop, moduli, n);
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

void *
xcalloc(size_t nmemb, size_t size)
{
    void *ptr;
    if ((ptr = calloc(nmemb, size)) == NULL) {
        fprintf(stderr, "%s: allocating %lu members of size %lu failed\n",
                fatalstr, nmemb, size);
        exit(1);
    }
    return ptr;
}

int
mpz_fread(mpz_t *x, FILE *fp)
{
    if (mpz_inp_raw(*x, fp) == 0) {
        fprintf(stderr, "%s: reading mpz failed\n", errorstr);
        return ERR;
    }
    return OK;
}

int
mpz_fwrite(mpz_t x, FILE *fp)
{
    if (mpz_out_raw(fp, x) == 0) {
        fprintf(stderr, "%s: writing mpz failed\n", errorstr);
        return ERR;
    }
    return OK;
}

int
int_fread(int *x, FILE *fp)
{
    if (fread(x, sizeof x[0], 1, fp) != 1) {
        fprintf(stderr, "%s: reading int failed\n", errorstr);
        return ERR;
    }
    return OK;
}

int
int_fwrite(int x, FILE *fp)
{
    if (fwrite(&x, sizeof x, 1, fp) != 1) {
        fprintf(stderr, "%s: writing int failed\n", errorstr);
        return ERR;
    }
    return OK;
}

int
size_t_fread(size_t *x, FILE *fp)
{
    if (fread(x, sizeof x[0], 1, fp) != 1) {
        fprintf(stderr, "%s: reading size_t failed\n", errorstr);
        return ERR;
    }
    return OK;
}

int
size_t_fwrite(size_t x, FILE *fp)
{
    if (fwrite(&x, sizeof x, 1, fp) != 1) {
        fprintf(stderr, "%s: writing size_t failed\n", errorstr);
        return ERR;
    }
    return OK;
}

int
bool_fread(bool *x, FILE *fp)
{
    if (fread(x, sizeof x[0], 1, fp) != 1) {
        fprintf(stderr, "%s: reading bool failed\n", errorstr);
        return ERR;
    }
    return OK;
}

int
bool_fwrite(bool x, FILE *fp)
{
    if (fwrite(&x, sizeof x, 1, fp) != 1) {
        fprintf(stderr, "%s: writing bool failed\n", errorstr);
        return ERR;
    }
    return OK;
}

char *
str_fread(FILE *fp)
{
    size_t len;
    char *str = NULL;
    if (size_t_fread(&len, fp) == ERR) goto error;
    str = xcalloc(len, sizeof str[0]);
    if (fread(str, 1, len, fp) != len) goto error;
    return str;
error:
    fprintf(stderr, "%s: reading string failed\n", errorstr);
    if (str)
        free(str);
    return NULL;
}

int
str_fwrite(const char *x, FILE *fp)
{
    if (size_t_fwrite(strlen(x) + 1, fp) == ERR) return ERR;
    if (fwrite(x, sizeof x[0], strlen(x) + 1, fp) != strlen(x) + 1) {
        fprintf(stderr, "%s: writing string failed\n", errorstr);
        return ERR;
    }
    return OK;
}


void
print_progress(size_t cur, size_t total)
{
    static const char *pbstr = "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";
    static int last_val = 0;
    const int pbwidth = 60;
    const double percentage = (double) cur / total;
    const int val  = percentage * 100;
    const int lpad = percentage * pbwidth;
    const int rpad = pbwidth - lpad;
    if (val != last_val) {
        fprintf(stderr, "\r\t%3d%% [%.*s%*s] %lu/%lu", val, lpad, pbstr, rpad, "", cur, total);
        if (cur == total)
            fprintf(stderr, "\n");
        fflush(stderr);
        last_val = val;
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
        fprintf(stderr, "%s: invalid input '%c'\n", errorstr, c);
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
    else {
        fprintf(stderr, "%s: unable to get file size of '%s'\n",
                warnstr, fname);
        return 0;
    }
}

char *
makestr(const char *fmt, ...)
{
    int length;
    char *str;
    va_list argp;
    va_start(argp, fmt);
    length = vsnprintf(NULL, 0, fmt, argp);
    va_end(argp);
    str = xcalloc(length + 1, sizeof str[0]);
    va_start(argp, fmt);
    (void) vsnprintf(str, length + 1, fmt, argp);
    va_end(argp);
    return str;
}

char *
longs_to_str(const long *xs, size_t n)
{
    char *str;
    str = xcalloc(sizeof(long) * (n + 1), sizeof str[0]);
    for (size_t i = 0; i < n; ++i)
        snprintf(&str[i], sizeof(long), "%ld", xs[i]);
    return str;
}

long *
str_to_longs(const char *str, size_t n)
{
    long *xs;
    xs = xcalloc(n, sizeof xs[0]);
    for (size_t i = 0; i < n; ++i)
        xs[i] = char_to_long(str[i]);
    return xs;
}

int
makedir(const char *dirname)
{
    DIR *dir = NULL;
    dir = opendir(dirname);
    if (dir) {
        closedir(dir);
    } else if (errno == ENOENT) {
        if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
            if (errno != EEXIST) {
                fprintf(stderr, "%s: unable to make directory '%s'\n",
                        errorstr, dirname);
                return ERR;
            }
        }
    } else {
        return ERR;
    }
    return OK;
}
