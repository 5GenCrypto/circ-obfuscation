#pragma once

#include <acirc.h>
#include <gmp.h>

int
circ_eval(acirc_t *circ, mpz_t *xs, size_t n, mpz_t *ys, size_t m,
          mpz_t modulus, size_t nthreads);
