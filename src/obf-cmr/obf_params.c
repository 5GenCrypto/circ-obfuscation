#include "obf_params.h"
#include "obfuscator.h"
#include "../mife-cmr/mife.h"
#include "../util.h"

size_t
mobf_num_encodings(const obf_params_t *op)
{
    const acirc_t *circ = op->circ;
    size_t count = 0;
    for (size_t i = 0; i < acirc_nslots(circ); ++i)
        count += acirc_symnum(circ, i) * mife_num_encodings_encrypt(circ, i);
    return count;
}

static void
_free(obf_params_t *op)
{
    if (op)
        free(op);
}

static obf_params_t *
_new(acirc_t *circ, void *vparams)
{
    const mobf_obf_params_t *params = vparams;
    obf_params_t *op;

    if ((op = my_calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->circ = circ;
    op->npowers = params->npowers;
    return op;
}

static void
_print(const obf_params_t *op)
{
    const acirc_t *circ = op->circ;
    const size_t nconsts = acirc_nconsts(circ) + acirc_nsecrets(circ);
    const size_t noutputs = acirc_noutputs(circ);
    const size_t has_consts = nconsts ? 1 : 0;
    size_t nencodings;
    nencodings = mobf_num_encodings(op)                     \
        + noutputs                                          \
        + acirc_nslots(circ) * op->npowers                  \
        + (has_consts ? acirc_symlen(circ, acirc_nsymbols(circ)) : 1);
    fprintf(stderr, "Obfuscation parameters:\n");
    fprintf(stderr, "* # powers: .. %lu\n", op->npowers);
    fprintf(stderr, "* # encodings: %lu\n", nencodings);
}

static int
_fwrite(const obf_params_t *op, FILE *fp)
{
    size_t_fwrite(op->npowers, fp);
    return OK;
}

static obf_params_t *
_fread(acirc_t *circ, FILE *fp)
{
    obf_params_t *op;

    op = my_calloc(1, sizeof op[0]);
    op->circ = circ;
    size_t_fread(&op->npowers, fp);
    return op;
}

op_vtable mobf_op_vtable =
{
    .new = _new,
    .free = _free,
    .fwrite = _fwrite,
    .fread = _fread,
    .print = _print,
};
