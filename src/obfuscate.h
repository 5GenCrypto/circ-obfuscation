#ifndef __SRC_OBFUSCATE__
#define __SRC_OBFUSCATE__

#include "mmap.h"

// for encodings parameterized by "s \in \Sigma", use the assignment as
// the index: s \in [2^q]
typedef struct {
    const mmap_vtable *mmap;
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

void
obfuscation_init(const mmap_vtable *mmap, obfuscation *obf, public_params *p);
void
obfuscation_clear(obfuscation *obf);

void
obfuscate(obfuscation *obf, secret_params *p, aes_randstate_t rng,
          bool rachel_input);

void
obfuscation_write(const obfuscation *obf, FILE *const fp);
void
obfuscation_read(obfuscation *obf, FILE *const fp, obf_params *p);
void obfuscation_eq (const obfuscation *x, const obfuscation *y);

#endif
