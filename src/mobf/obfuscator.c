#include "obfuscator.h"
#include "obf_params.h"
#include "input_chunker.h"
#include "../mife/mife.h"
#include "util.h"

#include <string.h>

typedef struct obfuscation {
    const obf_params_t *op;
    mife_t *mife;
    mife_ek_t *ek;
    mife_ciphertext_t ***cts;   /* [n][Σ] */
} obfuscation;

static void
_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const circ_params_t *const cp = &obf->op->cp;
    const size_t ninputs = cp->n;

    if (obf->ek)
        mife_ek_free(obf->ek);
    if (obf->cts) {
        for (size_t i = 0; i < ninputs; ++i) {
            for (size_t j = 0; j < cp->qs[i]; ++j) {
                mife_ciphertext_free(obf->cts[i][j], cp);
            }
            free(obf->cts[i]);
        }
        free(obf->cts);
    }
    if (obf->mife)
        mife_free(obf->mife);
    free(obf);
}

static obfuscation *
_obfuscate(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    obfuscation *obf;
    mife_sk_t *sk;
    mife_encrypt_pool_info_t pi;
    pthread_mutex_t lock;
    size_t count = 0;

    const circ_params_t *const cp = &op->cp;
    const size_t ninputs = cp->n;

    obf = my_calloc(1, sizeof obf[0]);
    obf->op = op;
    obf->mife = mife_setup(mmap, op, secparam, kappa, nthreads, rng);
    obf->ek = mife_ek(obf->mife);
    sk = mife_sk(obf->mife);
    obf->cts = my_calloc(ninputs, sizeof obf->cts[0]);

    pthread_mutex_init(&lock, NULL);
    pi.pool = threadpool_create(nthreads);
    pi.lock = &lock;
    pi.count = &count;
    pi.total = mobf_num_encodings(op);
    
    for (size_t i = 0; i < ninputs; ++i) {
        obf->cts[i] = my_calloc(cp->qs[i], sizeof obf->cts[i][0]);
        for (size_t j = 0; j < cp->qs[i]; ++j) {
            int inputs[cp->ds[i]];
            for (size_t k = 0; k < cp->ds[i]; ++k) {
                if (op->sigma)
                    inputs[k] = j == k;
                else if (cp->qs[i] == 2)
                    inputs[k] = bit(j, k);
                else {
                    if (cp->qs[i] != 1 && cp->ds[i] != 1) {
                        fprintf(stderr, "error: don't yet support base != 2 and symlen > 1\n");
                        goto error;
                    }
                    inputs[k] = j;
                }
            }
            obf->cts[i][j] = mife_encrypt(sk, i, inputs, op->npowers, nthreads,
                                          &pi, rng);
        }
    }
    threadpool_destroy(pi.pool);
    pthread_mutex_destroy(&lock);
    mife_sk_free(sk);
    return obf;
error:
    threadpool_destroy(pi.pool);
    pthread_mutex_destroy(&lock);
    mife_sk_free(sk);
    _free(obf);
    return NULL;
}

static int
_evaluate(const obfuscation *obf, int *rop, const int *inputs, size_t nthreads,
          size_t *kappa, size_t *max_npowers)
{
    (void) max_npowers;
    const circ_params_t *cp = &obf->op->cp;
    const acirc *const circ = cp->circ;
    const size_t has_consts = cp->c ? 1 : 0;
    const size_t ninputs = cp->n - has_consts;
    const size_t ell = array_max(cp->ds, ninputs);
    const size_t q = array_max(cp->qs, ninputs);
    int ret = ERR;

    mife_ciphertext_t **cts = NULL;
    int *input_syms = NULL;

    input_syms = get_input_syms(inputs, circ->ninputs, obf->op->rchunker,
                                cp->n - has_consts, ell, q, obf->op->sigma);
    if (input_syms == NULL)
        goto cleanup;
    cts = my_calloc(cp->n, sizeof cts[0]);
    for (size_t i = 0; i < cp->n - has_consts; ++i) {
        cts[i] = obf->cts[i][input_syms[i]];
    }
    if (has_consts)
        cts[cp->n - 1] = obf->cts[cp->n - 1][0];
    memset(rop, '\0', cp->m * sizeof rop[0]);
    if (mife_decrypt(obf->ek, rop, cts, nthreads, kappa) == ERR)
        goto cleanup;

    ret = OK;
cleanup:
    if (cts)
        free(cts);
    if (input_syms)
        free(input_syms);
    return ret;
}

static int
_fwrite(const obfuscation *const obf, FILE *const fp)
{
    const circ_params_t *cp = &obf->op->cp;
    const size_t ninputs = cp->n;
    mife_ek_fwrite(obf->ek, fp);
    for (size_t i = 0; i < ninputs; ++i) {
        for (size_t j = 0; j < cp->qs[i]; ++j) {
            mife_ciphertext_fwrite(obf->cts[i][j], cp, fp);
        }
    }
    return OK;
}

static obfuscation *
_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    obfuscation *obf;
    const circ_params_t *cp = &op->cp;
    const size_t ninputs = cp->n;
    obf = my_calloc(1, sizeof obf[0]);
    obf->op = op;
    if ((obf->ek = mife_ek_fread(mmap, op, fp)) == NULL)
        goto error;
    obf->mife = NULL;
    obf->cts = my_calloc(ninputs, sizeof obf->cts[0]);
    for (size_t i = 0; i < ninputs; ++i) {
        obf->cts[i] = my_calloc(cp->qs[i], sizeof obf->cts[i][0]);
        for (size_t j = 0; j < cp->qs[i]; ++j) {
            if ((obf->cts[i][j] = mife_ciphertext_fread(mmap, cp, fp)) == NULL)
                goto error;
        }
    }
    return obf;
error:
    _free(obf);
    return NULL;
}

obfuscator_vtable mobf_obfuscator_vtable = {
    .free = _free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _fwrite,
    .fread = _fread,
};
