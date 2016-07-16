#ifndef __SRC_OBFUSCATE__
#define __SRC_OBFUSCATE__

#include "mmap.h"

#include <aesrand.h>
#include <acirc.h>
#include <gmp.h>
#include <stddef.h>

// for encodings parameterized by "s \in \Sigma", use the assignment as
// the index: s \in [2^q]
typedef struct {
    obf_params *op;
    encoding *Zstar;
    encoding ***Rks;        // k \in [c], s \in \Sigma
    encoding ****Zksj;      // k \in [c], s \in \Sigma, j \in [\ell]
    encoding *Rc;
    encoding **Zcj;         // j \in [m] where m is length of secret P
    encoding ****Rhatkso;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding ****Zhatkso;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding **Rhato;       // o \in \Gamma
    encoding **Zhato;       // o \in \Gamma
    encoding **Rbaro;       // o \in \Gamma
    encoding **Zbaro;       // o \in \Gamma
} obfuscation;

void obfuscation_init  (obfuscation *obf, secret_params *p);
void obfuscation_clear (obfuscation *obf);

void obfuscate (
    obfuscation *obf,
    secret_params *s,
    aes_randstate_t rng
);

void encode_Zstar   (encoding *enc, secret_params *p, aes_randstate_t rng);
void encode_Rks     (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, size_t k, size_t s);
void encode_Zksj    (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, mpz_t ykj, size_t k, size_t s, size_t j);
void encode_Rc      (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs);
void encode_Zcj     (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, mpz_t ykj, int const_val);
void encode_Rhatkso (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, size_t k, size_t s, size_t o);
void encode_Zhatkso (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, mpz_t *whatk, size_t k, size_t s, size_t o);
void encode_Rhato   (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, size_t o);
void encode_Zhato   (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, mpz_t *what, size_t o);
void encode_Rbaro   (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, size_t o);
void encode_Zbaro   (encoding *enc, secret_params *p, aes_randstate_t rng, mpz_t *rs, mpz_t *what, mpz_t **whatk, mpz_t **ykj, acirc *c, size_t o);

void obfuscation_write (FILE *const fp, const obfuscation *obf);
void obfuscation_read  (obfuscation *ob, FILE *const fp, obf_params *op);
void obfuscation_eq (const obfuscation *x, const obfuscation *y);

#endif
