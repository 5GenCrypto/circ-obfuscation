#ifndef __LIN_FAKE_MMAP__
#define __LIN_FAKE_MMAP__

#define FAKE_MMAP 1

#include "level.h"
#include "obf_params.h"
#include "aesrand.h"
/* #if FAKE_MMAP */
/* #else */
/* #include <clt13.h> */
/* #endif */
#include <mmap/mmap.h>
#include <stdlib.h>

typedef struct {
    obf_params *op;
    level *toplevel;
    mmap_sk *sk;
} secret_params;

typedef struct {
    obf_params *op;
    level *toplevel;
    mmap_pp *pp;
} public_params;

typedef struct {
    level *lvl;
    size_t nslots;
    mmap_enc enc;
} encoding;

void
secret_params_init(const mmap_vtable *mmap, secret_params *p, obf_params *op,
                   level *toplevel, size_t lambda, aes_randstate_t rng);
void
secret_params_clear(const mmap_vtable *mmap, secret_params *p);

void
public_params_init(const mmap_vtable *mmap, public_params *p, secret_params *s);
void
public_params_clear(const mmap_vtable *mmap, public_params *p);

encoding *
encoding_new(const mmap_vtable *mmap, public_params *pp, obf_params *p);
void
encoding_free(const mmap_vtable *mmap, encoding *x);
void
encoding_set(const mmap_vtable *mmap, encoding *rop, encoding *x);

void
encode(const mmap_vtable *mmap, encoding *x, mpz_t *inps, size_t nins,
       const level *lvl, secret_params *sp);

int
encoding_mul(const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p);
int
encoding_add(const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p);
int
encoding_sub(const mmap_vtable *mmap, encoding *rop, encoding *x, encoding *y, public_params *p);
int
encoding_eq(encoding *x, encoding *y);

int
encoding_is_zero(const mmap_vtable *mmap, encoding *x, public_params *p);

void secret_params_read  (secret_params *x, FILE *const fp);
void secret_params_write (FILE *const fp, secret_params *x);
void public_params_read  (public_params *x, FILE *const fp);
void public_params_write (FILE *const fp, public_params *x);

void
encoding_read(const mmap_vtable *mmap, encoding *x, FILE *const fp);
void
encoding_write(const mmap_vtable *mmap, encoding *x, FILE *const fp);

#endif
