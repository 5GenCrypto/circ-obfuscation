#ifndef __SRC_OBFUSCATE__
#define __SRC_OBFUSCATE__

#include "circuit.h"
#include "level.h"
#include "input_chunker.h"
#include "clt13.h"
#include "fake_encoding.h"

// for encodings parameterized by "s \in \Sigma", use the assignment as
// the index: s \in [2^q]
typedef struct {
    params *p;
    encoding *Zstar;
    encoding ***Rsk;        // k \in [c], s \in \Sigma
    encoding ****Zsjk;      // k \in [c], s \in \Sigma, j \in [\ell]
    encoding *Rc;
    encoding **Zjc;         // j \in [m] where m is length of secret P
    encoding ****Rhatsok;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding ****Zhatsok;   // k \in [c], s \in \Sigma, o \in \Gamma
    encoding **Rhato;       // o \in \Gamma
    encoding **Zhato;       // o \in \Gamma
    encoding **Rbaro;       // o \in \Gamma
    encoding **Zbaro;       // o \in \Gamma
} obfuscation;

void obfuscation_init  (obfuscation *obf, params *p);
void obfuscation_clear (obfuscation *obf);

void obfuscate (obfuscation *obf, clt_state *mmap, circuit *c);

#endif
