#include "obf_params.h"
#include "obfuscator.h"
#include "../mife-cmr/mife.h"
#include "../util.h"

size_t
mobf_num_encodings(const obf_params_t *op)
{
    const circ_params_t *cp = &op->cp;
    size_t count = 0;
    for (size_t i = 0; i < cp->nslots; ++i)
        count += cp->qs[i] * mife_num_encodings_encrypt(cp->circ, i);
    return count;
}

static void
_free(obf_params_t *op)
{
    if (op) {
        circ_params_clear(&op->cp);
        free(op);
    }
}

static obf_params_t *
_new(acirc_t *circ, void *vparams)
{
    (void) circ;
    const mobf_obf_params_t *params = vparams;
    obf_params_t *op;

    if ((op = my_calloc(1, sizeof op[0])) == NULL)
        return NULL;
    op->npowers = params->npowers;
    return op;
}

static void
_print(const obf_params_t *op)
{
    const size_t nconsts = acirc_nconsts(op->cp.circ) + acirc_nsecrets(op->cp.circ);
    const size_t noutputs = acirc_noutputs(op->cp.circ);
    const size_t has_consts = nconsts ? 1 : 0;
    size_t nencodings;
    nencodings = mobf_num_encodings(op) \
        + noutputs \
        + op->cp.nslots * op->npowers \
        + (has_consts ? op->cp.ds[op->cp.nslots - 1] : 1);
    fprintf(stderr, "Obfuscation parameters:\n");
    fprintf(stderr, "* # powers: .. %lu\n", op->npowers);
    fprintf(stderr, "* # encodings: %lu\n", nencodings);
}

static int
_fwrite(const obf_params_t *op, FILE *fp)
{
    circ_params_fwrite(&op->cp, fp);
    size_t_fwrite(op->npowers, fp);
    return OK;
}

static obf_params_t *
_fread(acirc_t *circ, FILE *fp)
{
    obf_params_t *op;

    op = my_calloc(1, sizeof op[0]);
    circ_params_fread(&op->cp, circ, fp);
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
