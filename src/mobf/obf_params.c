#include "obf_params.h"
#include "obfuscator.h"

static obf_params_t *
_new(acirc *circ, void *vparams)
{
    return NULL;
}

static void
_free(obf_params_t *op)
{
    
}

op_vtable mife_op_vtable =
{
    .new = _new,
    .free = _free
};
