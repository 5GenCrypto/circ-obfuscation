#pragma once

#include <mmap/mmap.h>
#include "../mmap.h"

size_t polylog_nlevels(obf_params_t *op);
size_t polylog_nswitches(obf_params_t *op);
mmap_polylog_switch_params * polylog_switch_params(obf_params_t *op, size_t nzs);

