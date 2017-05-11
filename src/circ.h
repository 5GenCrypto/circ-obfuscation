#pragma once

#include <acirc.h>
#include <gmp.h>

int
circ_eval(acirc *circ, const mpz_t *xs, const mpz_t *ys, const mpz_t modulus,
          mpz_t *cache, size_t nthreads);
