#pragma once

#include <acirc.h>
#include <aesrand.h>
#include <mmap/mmap.h>
#include <stdlib.h>

#include "circ_params.h"

typedef struct obf_params_t obf_params_t;

typedef struct {
    obf_params_t * (*new)(acirc_t *, void *);
    void (*free)(obf_params_t *);
    int (*fwrite)(const obf_params_t *, FILE *);
    obf_params_t * (*fread)(acirc_t *, FILE *);
    void (*print)(const obf_params_t *);
} op_vtable;

obf_params_t * obf_params_new(const op_vtable *vt, acirc_t *circ, void *vparams);

typedef struct {
    size_t kappa;
    size_t nzs;
    size_t nslots;
    const int *pows;
} mmap_params_t;

typedef struct sp_info sp_info;
typedef struct {
    sp_info *info;
    mmap_sk sk;
} secret_params;

typedef struct {
    const mmap_vtable *mmap;
    int (*init)(secret_params *, mmap_params_t *, const obf_params_t *, size_t);
    int (*fwrite)(const secret_params *, FILE *);
    int (*fread)(secret_params *, const obf_params_t *, FILE *);
    void (*clear)(secret_params *);
    const void * (*toplevel)(const secret_params *);
} sp_vtable;

typedef struct pp_info pp_info;
typedef struct {
    pp_info *info;
    mmap_pp pp;
} public_params;

typedef struct {
    const mmap_vtable *mmap;
    int (*init)(const sp_vtable *, public_params *, const secret_params *,
                const obf_params_t *);
    int (*fwrite)(const public_params *, FILE *);
    int (*fread)(public_params *, const obf_params_t *, FILE *);
    void (*clear)(public_params *);
    const void * (*toplevel)(const public_params *);
} pp_vtable;

typedef struct encoding_info encoding_info;
typedef struct {
    encoding_info *info;
    mmap_enc enc;
} encoding;

typedef struct {
    const mmap_vtable *mmap;
    int (*new)(const pp_vtable *, encoding *, const public_params *);
    void (*free)(encoding *);
    int (*print)(const encoding *);
    int * (*encode)(encoding *, const void *);
    int (*set)(encoding *, const encoding *);
    int (*mul)(const pp_vtable *, encoding *, const encoding *,
               const encoding *, const public_params *);
    int (*add)(const pp_vtable *, encoding *, const encoding *,
               const encoding *, const public_params *);
    int (*sub)(const pp_vtable *, encoding *, const encoding *,
               const encoding *, const public_params *);
    int (*is_zero)(const pp_vtable *, const encoding *, const public_params *);
    int (*fread)(encoding *, FILE *);
    int (*fwrite)(const encoding *, FILE *);
    const void * (*mmap_set)(const encoding *);
} encoding_vtable;


secret_params * secret_params_new(const sp_vtable *vt, const obf_params_t *op,
                                  const acirc_t *circ, size_t lambda,
                                  size_t *kappa, size_t ncores, aes_randstate_t rng);
int             secret_params_fwrite(const sp_vtable *vt,
                                     const secret_params *sp, FILE *fp);
secret_params * secret_params_fread(const sp_vtable *vt, const obf_params_t *cp,
                                    FILE *fp);
void            secret_params_free(const sp_vtable *vt, secret_params *p);


public_params * public_params_new(const pp_vtable *vt, const sp_vtable *sp_vt,
                                  const secret_params *sp, const obf_params_t *op);
int             public_params_fwrite(const pp_vtable *vt,
                                     const public_params *pp, FILE *fp);
public_params * public_params_fread(const pp_vtable *vt, const obf_params_t *op,
                                    FILE *fp);
void            public_params_free(const pp_vtable *vt, public_params *p);

encoding * encoding_new(const encoding_vtable *enc_vt, const pp_vtable *pp_vt,
                        const public_params *pp);
void       encoding_free(const encoding_vtable *vt, encoding *x);
encoding * encoding_copy(const encoding_vtable *vt, const pp_vtable *pp_vt,
                         const public_params *pp, const encoding *enc);
int        encoding_print(const encoding_vtable *vt, const encoding *enc);
int        encode(const encoding_vtable *vt, encoding *rop, mpz_t *inps, size_t nins,
                  const void *set, const secret_params *sp, size_t level);
int        encoding_set(const encoding_vtable *vt, encoding *rop, const encoding *x);
int        encoding_mul(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
                        const encoding *x, const encoding *y, const public_params *p);
int        encoding_add(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
                        const encoding *x, const encoding *y, const public_params *p);
int        encoding_sub(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
                        const encoding *x, const encoding *y, const public_params *p);
unsigned int encoding_get_degree(const encoding_vtable *vt, const encoding *x);
int        encoding_is_zero(const encoding_vtable *vt, const pp_vtable *pp_vt,
                             const encoding *x, const public_params *p);
encoding * encoding_fread(const encoding_vtable *vt, FILE *fp);
int        encoding_fwrite(const encoding_vtable *vt, const encoding *x, FILE *fp);
