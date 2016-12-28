#include "obf_params.h"
#include "obfuscator.h"
#include "../mmap.h"
#include "../util.h"

#include <assert.h>

static obf_params_t *
_op_new(acirc *const circ, void *const vparams)
{
    zim_obf_params_t *const params = vparams;
    obf_params_t *op;

    op = calloc(1, sizeof(obf_params_t));
    op->ninputs = circ->ninputs;
    op->nconsts = circ->nconsts;
    op->noutputs = circ->noutputs;
    op->npowers = params->npowers;
    op->circ = circ;
    return op;
}

static void
_op_free(obf_params_t *op)
{
    free(op);
}

static int
_op_fwrite(const obf_params_t *const op, FILE *const fp)
{
    (void) op; (void) fp;
    assert(false);
    return OK;
}

static int
_op_fread(obf_params_t *const op, FILE *const fp)
{
    (void) op; (void) fp;
    assert(false);
    return OK;
}

op_vtable zim_op_vtable =
{
    .new = _op_new,
    .free = _op_free,
    .fwrite = _op_fwrite,
    .fread = _op_fread,
};
