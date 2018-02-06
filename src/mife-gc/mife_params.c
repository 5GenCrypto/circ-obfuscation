#include "mife_params.h"
#include "mife.h"
#include "../mife-cmr/mife.h"
#include "../util.h"

static obf_params_t *
_new(const acirc_t *circ, void *params_)
{
    mife_gc_params_t *params = params_;
    obf_params_t *op;

    if (params == NULL)
        return NULL;

    if ((op = my_calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->circ = circ;
    op->vt = &mife_cmr_vtable;
    op->padding = params->padding;
    op->wirelen = params->wirelen;
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
