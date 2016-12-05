#ifndef __MMAP_H__
#define __MMAP_H__

#include <acirc.h>
#include <aesrand.h>
#include <mmap/mmap.h>
#include <stdlib.h>

#include "input_chunker.h"

typedef struct obf_params_t obf_params_t;

obf_params_t *
obf_params_new(const acirc *const circ, input_chunker chunker,
               reverse_chunker rchunker, bool simple);

int
obf_params_fwrite(const obf_params_t *const op, FILE *const fp);

int
obf_params_fread(obf_params_t *const op, FILE *const fp);

void
obf_params_free(obf_params_t *p);


typedef struct sp_info sp_info;
typedef struct secret_params {
    sp_info *info;
    mmap_sk *sk;
} secret_params;

typedef struct {
    const mmap_vtable *mmap;
    int (*init)(const mmap_vtable *const, struct secret_params *const,
                 const obf_params_t *const, size_t, aes_randstate_t);
    void (*clear)(const mmap_vtable *const, struct secret_params *);
    void * (*toplevel)(const secret_params *const);
    void * (*params)(const secret_params *const);
} sp_vtable;

typedef struct pp_info pp_info;
typedef struct public_params {
    pp_info *info;
    mmap_pp *pp;
} public_params;

typedef struct {
    const mmap_vtable *mmap;
    void (*init)(const sp_vtable *const, public_params *const, const secret_params *const);
    void (*fwrite)(const public_params *const, FILE *const);
    void (*fread)(public_params *const, const obf_params_t *const, FILE *const);
    void (*clear)(public_params *const);
    void * (*toplevel)(const public_params *const);
    void * (*params)(const public_params *const);
} pp_vtable;

typedef struct encoding_info encoding_info;
typedef struct {
    encoding_info *info;
    mmap_enc *enc;
} encoding;

typedef struct {
    const mmap_vtable *mmap;
    int (*new)(const pp_vtable *const, encoding *const, const public_params *const);
    void (*free)(encoding *const);
    int (*print)(const encoding *const);
    int (*encode)(int *const, encoding *const, const void *const);
    int (*set)(encoding *const, const encoding *const);
    int (*mul)(const pp_vtable *const, encoding *const, const encoding *const,
               const encoding *const, const public_params *const);
    int (*add)(const pp_vtable *const, encoding *const, const encoding *const,
               const encoding *const, const public_params *const);
    int (*sub)(const pp_vtable *const, encoding *const, const encoding *const,
               const encoding *const, const public_params *const);
    int (*is_zero)(const pp_vtable *const, const encoding *const,
                   const public_params *const);
    void (*fread)(encoding *const, FILE *const);
    void (*fwrite)(const encoding *const, FILE *const);
} encoding_vtable;

int
secret_params_init(const sp_vtable *const vt, secret_params *const p,
                   const obf_params_t *const op, size_t lambda,
                   aes_randstate_t rng);
void
secret_params_clear(const sp_vtable *const vt, secret_params *const p);

void
public_params_init(const pp_vtable *const vt, const sp_vtable *const sp_vt,
                   public_params *const pp, const secret_params *const sp);
int
public_params_fwrite(const pp_vtable *const vt, const public_params *const pp,
                     FILE *const fp);
int
public_params_fread(const pp_vtable *const vt, public_params *const pp,
                    const obf_params_t *const op, FILE *const fp);
void
public_params_clear(const pp_vtable *vt, public_params *p);

encoding *
encoding_new(const encoding_vtable *const enc_vt, const pp_vtable *const pp_vt,
             const public_params *const pp);
void
encoding_free(const encoding_vtable *const vt, encoding *x);
int
encoding_print(const encoding_vtable *const vt, const encoding *const enc);

int
encode(const encoding_vtable *const vt, encoding *const rop,
       const mpz_t *const inps, size_t nins, const void *const set,
       const secret_params *const sp);

int
encoding_set(const encoding_vtable *const vt, encoding *const rop,
             const encoding *const x);
int
encoding_mul(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
             encoding *const rop, const encoding *const x,
             const encoding *const y, const public_params *const p);
int
encoding_add(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
             encoding *const rop, const encoding *const x,
             const encoding *const y, const public_params *const p);
int
encoding_sub(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
             encoding *const rop, const encoding *const x,
             const encoding *const y, const public_params *const p);
int
encoding_is_zero(const encoding_vtable *const vt, const pp_vtable *const pp_vt,
                 const encoding *const x, const public_params *const p);

void
encoding_fread(const encoding_vtable *const vt, encoding *x, FILE *const fp);
void
encoding_fwrite(const encoding_vtable *const vt, const encoding *const x,
                FILE *const fp);

#endif
