#include "obfuscator.h"
#include "obf_params.h"
#include "../mife-cmr/mife.h"
#include "../util.h"

#include <pthread.h>
#include <string.h>

typedef struct obfuscation {
    const acirc_t *circ;
    mife_t *mife;
    mife_ek_t *ek;
    mife_ct_t ***cts;   /* [n][Σ] */
} obfuscation;

static void
_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const acirc_t *circ = obf->circ;
    const size_t nslots = acirc_nslots(circ);
    const mife_vtable *vt = &mife_cmr_vtable;

    if (obf->ek)
        vt->mife_ek_free(obf->ek);
    if (obf->cts) {
        for (size_t i = 0; i < nslots; ++i) {
            if (obf->cts[i]) {
                for (size_t j = 0; j < acirc_symnum(circ, i); ++j) {
                    if (obf->cts[i][j])
                        vt->mife_ct_free(obf->cts[i][j]);
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
_obfuscate(const mmap_vtable *mmap, const acirc_t *circ, const obf_params_t *op,
           size_t secparam, size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    obfuscation *obf;
    mife_sk_t *sk;
    mife_encrypt_cache_t cache;
    pthread_mutex_t lock;
    size_t count = 0;
    double start, end, _start, _end;
    int res = ERR;

    const size_t nslots = acirc_nslots(circ);
    const mife_vtable *vt = &mife_cmr_vtable;

    start = _start = current_time();

    obf = xcalloc(1, sizeof obf[0]);
    obf->circ = circ;
    obf->mife = vt->mife_setup(mmap, circ, op, secparam, kappa, nthreads, rng);
    obf->ek = vt->mife_ek(obf->mife);
    sk = vt->mife_sk(obf->mife);
    obf->cts = xcalloc(nslots, sizeof obf->cts[0]);

    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "  MIFE setup: %.2fs\n", _end - _start);

    _start = current_time();

    pthread_mutex_init(&lock, NULL);
    cache.pool = threadpool_create(nthreads);
    cache.lock = &lock;
    cache.count = &count;
    cache.total = obf_cmr_num_encodings(circ);

    for (size_t i = 0; i < nslots; ++i) {
        obf->cts[i] = xcalloc(acirc_symnum(circ, i), sizeof obf->cts[i][0]);
        for (size_t j = 0; j < acirc_symnum(circ, i); ++j) {
            long *inputs;
            inputs = xcalloc(acirc_symlen(circ, i), sizeof inputs[0]);
            for (size_t k = 0; k < acirc_symlen(circ, i); ++k)
                inputs[k] = acirc_is_sigma(circ, i) ? j == k : j;
            obf->cts[i][j] = mife_cmr_encrypt(sk, i, inputs, acirc_symlen(circ, i),
                                              nthreads, rng, &cache, NULL);
            free(inputs);
        }
    }
    res = OK;
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
    const acirc_t *circ = obf->circ;
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

    if ((input_syms = get_input_syms(inputs, ninputs, circ)) == NULL)
        goto cleanup;
    cts = xcalloc(acirc_nslots(circ), sizeof cts[0]);
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        cts[i] = obf->cts[i][input_syms[i]];
    if (acirc_nconsts(circ))
        cts[acirc_nsymbols(circ)] = obf->cts[acirc_nsymbols(circ)][0];
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
    const acirc_t *circ = obf->circ;
    const mife_vtable *vt = &mife_cmr_vtable;
    vt->mife_ek_fwrite(obf->ek, fp);
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        for (size_t j = 0; j < acirc_symnum(circ, i); ++j) {
            vt->mife_ct_fwrite(obf->cts[i][j], fp);
        }
    }
    return OK;
}

static obfuscation *
_fread(const mmap_vtable *mmap, const acirc_t *circ, FILE *fp)
{
    const size_t nslots = acirc_nslots(circ);
    const mife_vtable *vt = &mife_cmr_vtable;
    obfuscation *obf = NULL;

    obf = xcalloc(1, sizeof obf[0]);
    if ((obf->ek = vt->mife_ek_fread(mmap, circ, fp)) == NULL) goto error;
    obf->mife = NULL;
    obf->circ = circ;
    obf->cts = xcalloc(nslots, sizeof obf->cts[0]);
    for (size_t i = 0; i < nslots; ++i) {
        obf->cts[i] = xcalloc(acirc_symnum(circ, i), sizeof obf->cts[i][0]);
        for (size_t j = 0; j < acirc_symnum(circ, i); ++j) {
            if ((obf->cts[i][j] = vt->mife_ct_fread(mmap, obf->ek, fp)) == NULL)
                goto error;
        }
    }
    return obf;
error:
    if (obf)
        _free(obf);
    return NULL;
}

obfuscator_vtable obf_cmr_vtable = {
    .free = _free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _fwrite,
    .fread = _fread,
};
