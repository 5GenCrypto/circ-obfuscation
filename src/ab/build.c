/* We want to build as a static library, but only export a couple of functions.
   Including all the necessary files seems like the best way to do this... */
#include "vtables.h"
#include "obf_params.h"
#include "obf_params.c"
#include "level.c"
#include "secret_params.c"
#include "public_params.c"
#include "encoding.c"
#include "obfuscator.c"
