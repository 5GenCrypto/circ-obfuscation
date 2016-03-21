#ifndef __SRC_OBFUSCATE__
#define __SRC_OBFUSCATE__

#include "circuit.h"
#include "clt13.h"
#include "fake_encoding.h"
#include "level.h"

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

void obfuscation_init  (obfuscation *obf, fake_params *p);
void obfuscation_clear (obfuscation *obf);

void obfuscate (
    obfuscation *obf,
    fake_params *p,
    circuit *circ,
    gmp_randstate_t *rng
);

void encode_Zstar (encoding *enc, fake_params *p, gmp_randstate_t *rng);
void encode_Rks   (encoding *enc, fake_params *p, mpz_t *rs, size_t k, size_t s);

void encode_Zksj (
    encoding *enc,
    fake_params *p,
    mpz_t *rs,
    mpz_t ykj,
    size_t k,
    size_t s,
    size_t j,
    gmp_randstate_t *rng
);

void encode_Rc      (encoding *enc, fake_params *p, mpz_t *rs);
void encode_Zcj     (encoding *enc, fake_params *p, mpz_t *rs, mpz_t ykj, int const_val, gmp_randstate_t *rng);
void encode_Rhatkso (encoding *enc, fake_params *p, mpz_t *rs, size_t k, size_t s, size_t o);
void encode_Zhatkso (encoding *enc, fake_params *p, mpz_t *rs, mpz_t *w, size_t k, size_t s, size_t o);
void encode_Rhato   (encoding *enc, fake_params *p, mpz_t *rs, size_t o);
void encode_Zhato   (encoding *enc, fake_params *p, mpz_t *rs, mpz_t **whatk, mpz_t **ykj, circuit *c, size_t o);
void encode_Rbaro   (encoding *enc, fake_params *p, mpz_t *rs, size_t o);
void encode_Zbaro   (encoding *enc, fake_params *p, mpz_t *rs, mpz_t **whatk, mpz_t **ykj, circuit *c, size_t o);

#endif
