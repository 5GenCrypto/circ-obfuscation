#ifndef __AB__MMAP_H__
#define __AB__MMAP_H__

#include "level.h"
#include "obf_params.h"

#include <aesrand.h>
#include <mmap/mmap.h>
#include <stdlib.h>

typedef struct {
    const obf_params_t *op;
    level *toplevel;
    mmap_sk *sk;
} secret_params;

typedef struct {
    obf_params_t *op;
    level *toplevel;
    mmap_pp *pp;
} public_params;

typedef struct {
    level *lvl;
    mmap_enc enc;
} encoding;

void
secret_params_init(const mmap_vtable *mmap, secret_params *p,
                   const obf_params_t *const op, size_t lambda,
                   aes_randstate_t rng);
void
secret_params_clear(const mmap_vtable *mmap, secret_params *p);

void
public_params_init(const mmap_vtable *mmap, public_params *p, secret_params *s);
int
public_params_fwrite(const mmap_vtable *mmap, const public_params *const pp,
                     FILE *const fp);
int
public_params_fread(const mmap_vtable *const mmap, public_params *pp,
                    const obf_params_t *op, FILE *const fp);
void
public_params_clear(const mmap_vtable *mmap, public_params *p);

encoding *
encoding_new(const mmap_vtable *mmap, public_params *pp);
void
encoding_free(const mmap_vtable *mmap, encoding *x);
void
encoding_print(const mmap_vtable *const mmap, encoding *enc);
void
encoding_set(const mmap_vtable *mmap, encoding *rop, encoding *x);

void
encode(const mmap_vtable *const mmap, encoding *x, mpz_t *inps, size_t nins,
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

void
encoding_fread(const mmap_vtable *const mmap, encoding *x, FILE *const fp);
void
encoding_fwrite(const mmap_vtable *const mmap, const encoding *const x, FILE *const fp);

#endif
