#include "mife_params.h"
#include "mife.h"
#include "../mife-cmr/mife.h"
#include "../util.h"

static obf_params_t *
_new(const acirc_t *circ, void *vparams)
{
    mife_gc_params_t *params = vparams;
    obf_params_t *op;

    if ((op = my_calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->circ = circ;
    op->vt = &mife_cmr_vtable;
    op->indexed = params ? params->indexed : false;
    op->padding = params ? params->padding : 10;
    return op;
}

static void
_free(obf_params_t *op)
{
    free(op);
}

op_vtable mife_gc_op_vtable =
{
    .new = _new,
    .free = _free,
};
