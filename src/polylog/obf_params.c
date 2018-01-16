#include "obf_params.h"
#include "obfuscator.h"
#include "../util.h"

PRIVATE size_t
obf_params_nzs(const circ_params_t *cp)
{
    return 2 * acirc_ninputs(cp->circ) + 1;
}

PRIVATE index_set *
obf_params_new_toplevel(const circ_params_t *const cp, size_t nzs)
{
    (void) cp;
    index_set *ix;

    if ((ix = index_set_new(nzs)) == NULL)
        return NULL;
    /* IX_Z(ix) = 1; */
    /* for (size_t i = 0; i < acirc_ninputs(cp->circ); ++i) { */
    /*     IX_W(ix, cp, i) = 1; */
    /*     IX_X(ix, cp, i) = acirc_max_var_degree(cp->circ, i); */
    /* } */
    return ix;
}

static obf_params_t *
_new(acirc_t *circ, void *vparams)
{
    (void) vparams;
    obf_params_t *op;
    const size_t ninputs = acirc_ninputs(circ);

    op = calloc(1, sizeof op[0]);
    circ_params_init(&op->cp, ninputs, circ);
    if (g_verbose)
        circ_params_print(&op->cp);
    op->nlevels = acirc_max_depth(op->cp.circ) + 1;
    op->nswitches = acirc_nrefs(circ) + 1 + acirc_ninputs(circ);

    return op;
}

static void
_free(obf_params_t *op)
{
    circ_params_clear(&op->cp);
    free(op);
}

op_vtable polylog_op_vtable =
{
    .new = _new,
    .free = _free,
};
