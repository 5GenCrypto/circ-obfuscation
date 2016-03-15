#ifndef __OBFUSCATE__
#define __OBFUSCATE__

#include "circuit.h"
#include "level.h"
#include "input_chunker.h"
#include "clt13.h"

typedef struct {
    params *p;
    encoding Zstar;
    encoding **Rsk;
    encoding ***Zsjk;
    encoding Rc;
    encoding *Zjc;
    encoding ***Rhatsok;
    encoding ***Zhatsok;
    encoding *Rhato;
    encoding *Zhato;
    encoding *Rbaro;
    encoding *Zbaro;
} obfuscation;

void obfuscation_init  (obfuscation *obf, params *p);
void obfuscation_clear (obfuscation *obf);

void obfuscate (obfuscation *obf, clt_state *mmap, circuit *c);

#endif
