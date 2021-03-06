#pragma once

#include <acirc.h>
#include <stddef.h>

typedef struct circ_params_t {
    size_t n;                   /* number of input strings */
    size_t c;                   /* number of constant bits */
    size_t m;                   /* number of output bits */
    size_t *ds;                 /* number of bits in each input string */
    size_t *qs;                 /* number of symbols associated with input string */
    acirc *circ;
} circ_params_t;

int
circ_params_init(circ_params_t *cp, size_t n, acirc *circ);
void
circ_params_clear(circ_params_t *cp);
int
circ_params_fwrite(const circ_params_t *const cp, FILE *fp);
int
circ_params_fread(circ_params_t *const cp, acirc *circ, FILE *fp);
size_t
circ_params_ninputs(const circ_params_t *cp);
size_t
circ_params_slot(const circ_params_t *cp, size_t pos);
size_t
circ_params_bit(const circ_params_t *cp, size_t pos);
void
circ_params_print(const circ_params_t *cp);
