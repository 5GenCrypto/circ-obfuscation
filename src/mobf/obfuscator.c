#include "obfuscator.h"
#include "obf_params.h"
#include "input_chunker.h"
#include "../mife/mife.h"
#include "util.h"

typedef struct obfuscation {
    const obf_params_t *op;
    mife_ek_t *ek;
    mife_ciphertext_t ***cts;   /* [n][Σ] */
} obfuscation;

static void
_free(obfuscation *obf)
{
    /* XXX */
}

static obfuscation *
_obfuscate(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    obfuscation *obf;
    mife_t *mife;
    mife_sk_t *sk;

    const circ_params_t *const cp = &op->cp;
    acirc *const circ = cp->circ;
    const size_t ninputs = cp->n - 1;
    const size_t nconsts = cp->ds[ninputs];
    const size_t noutputs = cp->m;

    obf = my_calloc(1, sizeof obf[0]);
    obf->op = op;
    mife = mife_setup(mmap, cp, secparam, rng, kappa, nthreads);
    obf->ek = mife_ek(mife);
    sk = mife_sk(mife);

    obf->cts = my_calloc(ninputs, sizeof obf->cts[0]);
    for (size_t i = 0; i < ninputs; ++i) {
        obf->cts[i] = my_calloc(cp->qs[i], sizeof obf->cts[i][0]);
        for (size_t j = 0; j < cp->qs[i]; ++j) {
            int input = j;
            obf->cts[i][j] = mife_encrypt(sk, i, &input, op->npowers, nthreads, rng);
        }
    }

    return obf;
}

static int
_evaluate(const obfuscation *obf, int *rop, const int *inputs, size_t nthreads,
          size_t *kappa, size_t *max_npowers)
{
    const circ_params_t *cp = &obf->op->cp;
    const acirc *const circ = cp->circ;
    const size_t ninputs = cp->n - 1;
    const size_t ell = array_max(cp->ds, ninputs);
    const size_t q = array_max(cp->qs, ninputs);

    mife_ciphertext_t **cts;
    int *input_syms;

    input_syms = get_input_syms(inputs, circ->ninputs, obf->op->rchunker,
                                ninputs, ell, q, obf->op->sigma);
    if (input_syms == NULL)
        return ERR;
    cts = my_calloc(ninputs, sizeof cts[0]);

    for (size_t i = 0; i < ninputs; ++i) {
        cts[i] = obf->cts[i][input_syms[i]];
    }

    if (mife_decrypt(obf->ek, rop, cts, nthreads, kappa) == ERR)
        return ERR;

    return OK;
}

static int
_fwrite(const obfuscation *const obf, FILE *const fp)
{
    return ERR;
}

static obfuscation *
_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    return NULL;
}

obfuscator_vtable mife_obfuscator_vtable = {
    .free = _free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _fwrite,
    .fread = _fread,
};
