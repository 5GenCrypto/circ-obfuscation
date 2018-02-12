#include "mife_params.h"
#include "mife.h"
#include "../mife-cmr/mife.h"
#include "../util.h"

static obf_params_t *
mife_gc_op_new(const acirc_t *circ, void *params_)
{
    mife_gc_params_t *params = params_;
    obf_params_t *op;

    if (params == NULL)
        return NULL;

    op = xcalloc(1, sizeof op[0]);
    op->circ = circ;
    op->vt = &mife_cmr_vtable;
    op->npowers = params->npowers;
    op->padding = params->padding;
    op->wirelen = params->wirelen;
    op->ngates = params->ngates ? params->ngates : acirc_nmuls(circ);
    return op;
}

static void
mife_gc_op_free(obf_params_t *op)
{
    free(op);
}

static void
mife_gc_op_print(const obf_params_t *op)
{
    fprintf(stderr, "MIFE parameters:\n");
    fprintf(stderr, "———— # powers: ..... %lu\n", op->npowers);
    fprintf(stderr, "———— padding: ...... %lu\n", op->padding);
    fprintf(stderr, "———— wire length: .. %lu\n", op->wirelen);
    fprintf(stderr, "———— # gates / GC: . %lu\n", op->ngates);
}

op_vtable mife_gc_op_vtable =
{
    .new = mife_gc_op_new,
    .free = mife_gc_op_free,
    .print = mife_gc_op_print,
};
