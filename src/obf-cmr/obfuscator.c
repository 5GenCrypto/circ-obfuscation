#include "obfuscator.h"
#include "obf_params.h"
#include "../mife-cmr/mife.h"
#include "../util.h"

#include <string.h>

typedef struct obfuscation {
    const obf_params_t *op;
    mife_t *mife;
    mife_ek_t *ek;
    mife_ct_t ***cts;   /* [n][Σ] */
} obfuscation;

static void
_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const circ_params_t *cp = obf_params_cp(obf->op);
    const size_t ninputs = cp->nslots;
    const mife_vtable *vt = &mife_cmr_vtable;

    if (obf->ek)
        vt->mife_ek_free(obf->ek);
    if (obf->cts) {
        for (size_t i = 0; i < ninputs; ++i) {
            if (obf->cts[i]) {
                for (size_t j = 0; j < cp->qs[i]; ++j) {
                    if (obf->cts[i][j])
                        vt->mife_ct_free(obf->cts[i][j], cp);
                }
                free(obf->cts[i]);
            }
        }
        free(obf->cts);
    }
    if (obf->mife)
        vt->mife_free(obf->mife);
    free(obf);
}

static obfuscation *
_obfuscate(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    obfuscation *obf;
    mife_sk_t *sk;
    mife_encrypt_cache_t cache;
    pthread_mutex_t lock;
    size_t count = 0;
    double start, end, _start, _end;
    int res = ERR;

    const circ_params_t *cp = obf_params_cp(op);
    const size_t ninputs = cp->nslots;
    const mife_vtable *vt = &mife_cmr_vtable;

    /* MIFE setup */
    start = _start = current_time();

    obf = my_calloc(1, sizeof obf[0]);
    obf->op = op;
    obf->mife = vt->mife_setup(mmap, op, secparam, kappa, op->npowers, nthreads, rng);
    obf->ek = vt->mife_ek(obf->mife);
    sk = vt->mife_sk(obf->mife);
    obf->cts = my_calloc(ninputs, sizeof obf->cts[0]);

    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "  MIFE setup: %.2fs\n", _end - _start);

    /* MIFE encryption */
    _start = current_time();

    pthread_mutex_init(&lock, NULL);
    cache.pool = threadpool_create(nthreads);
    cache.lock = &lock;
    cache.count = &count;
    cache.total = mobf_num_encodings(op);

    for (size_t i = 0; i < ninputs; ++i) {
        obf->cts[i] = my_calloc(cp->qs[i], sizeof obf->cts[i][0]);
        for (size_t j = 0; j < cp->qs[i]; ++j) {
            long inputs[cp->ds[i]];
            for (size_t k = 0; k < cp->ds[i]; ++k) {
                if (acirc_is_sigma(cp->circ, i))
                    inputs[k] = j == k;
                else if (cp->qs[i] == 2)
                    inputs[k] = bit(j, k);
                else {
                    if (cp->qs[i] != 1 && cp->ds[i] != 1) {
                        fprintf(stderr, "error: don't yet support base != 2 and symlen > 1\n");
                        goto cleanup;
                    }
                    inputs[k] = j;
                }
            }
            obf->cts[i][j] = _mife_encrypt(sk, i, inputs, nthreads, rng, &cache, NULL, false);
        }
    }
    res = OK;
cleanup:
    threadpool_destroy(cache.pool);
    pthread_mutex_destroy(&lock);
    vt->mife_sk_free(sk);
    if (res == OK) {
        end = _end = current_time();
        if (g_verbose) {
            fprintf(stderr, "  MIFE encrypt: %.2fs\n", _end - _start);
            fprintf(stderr, "  Obfuscate: %.2fs\n", end - start);
        }
        return obf;
    } else {
        _free(obf);
        return NULL;
    }
}

static int
_evaluate(const obfuscation *obf, long *outputs, size_t noutputs,
          const long *inputs, size_t ninputs, size_t nthreads, size_t *kappa,
          size_t *npowers)
{
    (void) npowers;
    const circ_params_t *cp = obf_params_cp(obf->op);
    const acirc_t *circ = cp->circ;
    const size_t has_consts = acirc_nconsts(circ) + acirc_nsecrets(circ) ? 1 : 0;
    const mife_vtable *vt = &mife_cmr_vtable;
    mife_ct_t **cts = NULL;
    size_t *input_syms = NULL;
    int ret = ERR;

    if (ninputs != acirc_ninputs(circ)) {
        fprintf(stderr, "error: obf evaluate: invalid number of inputs\n");
        goto cleanup;
    } else if (noutputs != acirc_noutputs(circ)) {
        fprintf(stderr, "error: obf evaluate: invalid number of outputs\n");
        goto cleanup;
    }

    {
        bool *sigmas;
        sigmas = my_calloc(cp->nslots - has_consts, sizeof sigmas[0]);
        for (size_t i = 0; i < cp->nslots - has_consts; ++i)
            sigmas[i] = acirc_is_sigma(circ, i);
        input_syms = get_input_syms(inputs, ninputs, cp->nslots - has_consts,
                                    cp->ds, cp->qs, sigmas);
        free(sigmas);
        if (input_syms == NULL)
            goto cleanup;
    }
    cts = my_calloc(cp->nslots, sizeof cts[0]);
    for (size_t i = 0; i < cp->nslots - has_consts; ++i) {
        cts[i] = obf->cts[i][input_syms[i]];
    }
    if (has_consts)
        cts[cp->nslots - 1] = obf->cts[cp->nslots - 1][0];
    if (vt->mife_decrypt(obf->ek, outputs, (const mife_ct_t **) cts, nthreads, kappa) == ERR)
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
    const size_t ninputs = cp->nslots;
    const mife_vtable *vt = &mife_cmr_vtable;
    vt->mife_ek_fwrite(obf->ek, fp);
    for (size_t i = 0; i < ninputs; ++i) {
        for (size_t j = 0; j < cp->qs[i]; ++j) {
            vt->mife_ct_fwrite(obf->cts[i][j], cp, fp);
        }
    }
    return OK;
}

static obfuscation *
_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    obfuscation *obf;
    const circ_params_t *cp = obf_params_cp(op);
    const size_t ninputs = cp->nslots;
    const mife_vtable *vt = &mife_cmr_vtable;
    obf = my_calloc(1, sizeof obf[0]);
    obf->op = op;
    if ((obf->ek = vt->mife_ek_fread(mmap, op, fp)) == NULL)
        goto error;
    obf->mife = NULL;
    obf->cts = my_calloc(ninputs, sizeof obf->cts[0]);
    for (size_t i = 0; i < ninputs; ++i) {
        obf->cts[i] = my_calloc(cp->qs[i], sizeof obf->cts[i][0]);
        for (size_t j = 0; j < cp->qs[i]; ++j) {
            if ((obf->cts[i][j] = vt->mife_ct_fread(mmap, cp, fp)) == NULL)
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
