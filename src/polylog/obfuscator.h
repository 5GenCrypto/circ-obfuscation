#pragma once

#include "../obfuscator.h"

/* typedef struct { */
/*     size_t npowers; */
/*     bool sigma; */
/*     size_t base; */
/*     size_t nlevels; */
/* } polylog_obf_params_t; */

extern obfuscator_vtable polylog_obfuscator_vtable;
extern op_vtable polylog_op_vtable;
