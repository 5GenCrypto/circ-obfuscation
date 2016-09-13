#ifndef __SRC_OBFUSCATE__
#define __SRC_OBFUSCATE__

#include "mmap.h"

typedef struct {
    const mmap_vtable *mmap;
    obf_params *op;
    encoding ***R_ib;
    encoding ***Z_ib;
    encoding ***R_hat_ib;
    encoding ***Z_hat_ib;
    encoding **R_i;
    encoding **Z_i;
    encoding **R_o_i;
    encoding **Z_o_i;
} obfuscation;

obfuscation *
obfuscation_new(const mmap_vtable *mmap, public_params *p);
void
obfuscation_free(obfuscation *obf);

void
obfuscate(obfuscation *obf, secret_params *p, aes_randstate_t rng);

void
obfuscation_write(const obfuscation *obf, FILE *const fp);
void
obfuscation_read(obfuscation *obf, FILE *const fp, obf_params *p);
void
obfuscation_eq(const obfuscation *x, const obfuscation *y);

#endif
