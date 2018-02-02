#pragma once

#include <acirc.h>
#include <stddef.h>

size_t circ_params_ninputs(const acirc_t *circ);
size_t circ_params_slot(const acirc_t *circ, size_t pos);
size_t circ_params_bit(const acirc_t *circ, size_t pos);
int    circ_params_print(const acirc_t *circ);
size_t * get_input_syms(const long *inputs, size_t ninputs, const acirc_t *circ);
