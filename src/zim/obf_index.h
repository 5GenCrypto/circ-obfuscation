#ifndef __ZIM__OBF_INDEX__
#define __ZIM__OBF_INDEX__

#include <stdio.h>

#define IX_Y(IX)       ((IX)->pows[0])
#define IX_X(IX, I, B) ((IX)->pows[(1 + 2*(I) + (B))])
#define IX_Z(IX, I)    ((IX)->pows[(1 + (2*(IX)->n) + (I))])
#define IX_W(IX, I)    ((IX)->pows[(1 + (3*(IX)->n) + (I))])

typedef struct {
    unsigned long *pows;
    size_t nzs;
    size_t n;       // number of inputs to the circuit
} obf_index;

#endif
