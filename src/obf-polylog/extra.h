#pragma once

#include <mmap/mmap.h>
#include "../mmap.h"

mmap_sk
polylog_secret_params_new(const sp_vtable *vt, const obf_params_t *op,
                          mmap_sk_params *p, mmap_sk_opt_params *o,
                          const mmap_params_t *params, size_t ncores,
                          aes_randstate_t rng);
