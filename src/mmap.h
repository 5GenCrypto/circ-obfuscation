#ifndef __MMAP_H__
#define __MMAP_H__

#include <acirc.h>
#include <aesrand.h>
#include <mmap/mmap.h>
#include <stdlib.h>

typedef struct obf_params_t obf_params_t;
typedef struct {
    obf_params_t * (*new)(acirc *, void *);
    void (*free)(obf_params_t *);
    int (*fwrite)(const obf_params_t *, FILE *);
    int (*fread)(obf_params_t *, FILE *);
} op_vtable;

typedef struct mmap_params_t {
    size_t kappa;
    size_t nzs;
    size_t nslots;
    size_t *pows;
    bool my_pows;
} mmap_params_t;

void
mmap_params_fprint(FILE *fp, const mmap_params_t *params);

typedef struct sp_info sp_info;
typedef struct secret_params {
    mmap_sk *sk;
    sp_info *info;
} secret_params;

typedef struct {
    const mmap_vtable *mmap;
    mmap_params_t (*init)(struct secret_params *, const obf_params_t *, size_t);
    void (*clear)(struct secret_params *);
    const void * (*toplevel)(const secret_params *);
    const void * (*params)(const secret_params *);
} sp_vtable;

typedef struct pp_info pp_info;
typedef struct public_params {
    pp_info *info;
    mmap_pp *pp;
    bool my_pp;
} public_params;

typedef struct {
    const mmap_vtable *mmap;
    void (*init)(const sp_vtable *, public_params *, const secret_params *);
    void (*fwrite)(const public_params *, FILE *);
    void (*fread)(public_params *, const obf_params_t *, FILE *);
    void (*clear)(public_params *);
    const void * (*toplevel)(const public_params *);
    const void * (*params)(const public_params *);
} pp_vtable;

typedef struct encoding_info encoding_info;
typedef struct {
    encoding_info *info;
    mmap_enc *enc;
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
    void (*fread)(encoding *, FILE *);
    void (*fwrite)(const encoding *, FILE *);
    const void * (*mmap_set)(const encoding *);
} encoding_vtable;

int
secret_params_init(const sp_vtable *vt, secret_params *p, const obf_params_t *op,
                   size_t lambda, size_t kappa, aes_randstate_t rng);
void
secret_params_clear(const sp_vtable *vt, secret_params *p);

void
public_params_init(const pp_vtable *vt, const sp_vtable *sp_vt,
                   public_params *pp, const secret_params *sp);
int
public_params_fwrite(const pp_vtable *vt, const public_params *pp, FILE *fp);
int
public_params_fread(const pp_vtable *vt, public_params *pp,
                    const obf_params_t *op, FILE *fp);
void
public_params_clear(const pp_vtable *vt, public_params *p);

encoding *
encoding_new(const encoding_vtable *enc_vt, const pp_vtable *pp_vt,
             const public_params *pp);
void
encoding_free(const encoding_vtable *vt, encoding *x);
int
encoding_print(const encoding_vtable *vt, const encoding *enc);

int
encode(const encoding_vtable *vt, encoding *rop, mpz_t *inps, size_t nins,
       const void *set, const secret_params *sp);

int
encoding_set(const encoding_vtable *vt, encoding *rop, const encoding *x);
int
encoding_mul(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
             const encoding *x, const encoding *y, const public_params *p);
int
encoding_add(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
             const encoding *x, const encoding *y, const public_params *p);
int
encoding_sub(const encoding_vtable *vt, const pp_vtable *pp_vt, encoding *rop,
             const encoding *x, const encoding *y, const public_params *p);
int
encoding_is_zero(const encoding_vtable *vt, const pp_vtable *pp_vt,
                 const encoding *x, const public_params *p);
void
encoding_fread(const encoding_vtable *vt, encoding *x, FILE *fp);
void
encoding_fwrite(const encoding_vtable *vt, const encoding *x, FILE *fp);

#endif
