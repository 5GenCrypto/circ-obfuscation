#include "mife.h"

#include "../index_set.h"
#include "mife_params.h"
#include "../vtables.h"
#include "../util.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

struct mife_t {
    const mmap_vtable *mmap;
    const obf_params_t *op;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    secret_params *sp;
    public_params *pp;
    size_t npowers;
    encoding *Chatstar;
    /* XXX may want to bring back [noutputs] number of zhats, as with only a
     * single one we may get the scenario where we need to do multiple
     * `raise_encodings` operations when computing the LHS of the final
     * comparison equation. */
    encoding *zhat;
    encoding ***uhat;           /* [n][npowers] */
    mife_ct_t *constants;
    mpz_t *const_alphas;
    size_t *deg_max;            /* [n] */
};

struct mife_ct_t {
    const encoding_vtable *enc_vt;
    const acirc_t *circ;
    size_t slot;
    encoding **xhat;             /* [d_i] */
    encoding **what;             /* [m] */
};

struct mife_sk_t {
    const mmap_vtable *mmap;
    const acirc_t *circ;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    secret_params *sp;
    public_params *pp;
    mpz_t *const_alphas;
    size_t *deg_max;            /* [n] */
    bool local;
};

struct mife_ek_t {
    const mmap_vtable *mmap;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    const acirc_t *circ;
    public_params *pp;
    size_t npowers;
    encoding *Chatstar;
    encoding *zhat;
    encoding ***uhat;           /* [n][npowers] */
    mife_ct_t *constants;
    size_t *deg_max;            /* [n] */
    bool local;
};

static void
mife_ct_free(mife_ct_t *ct)
{
    if (ct == NULL) return;
    if (ct->xhat) {
        for (size_t j = 0; j < acirc_symlen(ct->circ, ct->slot); ++j)
            if (ct->xhat[j])
                encoding_free(ct->enc_vt, ct->xhat[j]);
        free(ct->xhat);
    }
    if (ct->what) {
        for (size_t o = 0; o < acirc_noutputs(ct->circ); ++o)
            if (ct->what[o])
                encoding_free(ct->enc_vt, ct->what[o]);
        free(ct->what);
    }
    free(ct);
}

static int
mife_ct_fwrite(const mife_ct_t *ct, FILE *fp)
{
    const size_t ninputs = acirc_symlen(ct->circ, ct->slot);
    if (size_t_fwrite(ct->slot, fp) == ERR) return ERR;
    for (size_t j = 0; j < ninputs; ++j)
        if (encoding_fwrite(ct->enc_vt, ct->xhat[j], fp) == ERR) return ERR;
    for (size_t o = 0; o < acirc_noutputs(ct->circ); ++o)
        if (encoding_fwrite(ct->enc_vt, ct->what[o], fp) == ERR) return ERR;
    return OK;
}

static mife_ct_t *
mife_ct_fread(const mmap_vtable *mmap, const mife_ek_t *ek, FILE *fp)
{
    mife_ct_t *ct;
    size_t ninputs;

    ct = xcalloc(1, sizeof ct[0]);
    ct->enc_vt = get_encoding_vtable(mmap);
    if (size_t_fread(&ct->slot, fp) == ERR) goto error;
    if (ct->slot >= acirc_nslots(ek->circ)) {
        fprintf(stderr, "%s: %s: slot number > number of slots\n",
                errorstr, __func__);
        goto error;
    }
    ct->circ = ek->circ;
    ninputs = acirc_symlen(ct->circ, ct->slot);
    ct->xhat = xcalloc(ninputs, sizeof ct->xhat[0]);
    for (size_t j = 0; j < ninputs; ++j) {
        if ((ct->xhat[j] = encoding_fread(ct->enc_vt, fp)) == NULL)
            goto error;
    }
    ct->what = xcalloc(acirc_noutputs(ct->circ), sizeof ct->what[0]);
    for (size_t o = 0; o < acirc_noutputs(ct->circ); ++o)
        if ((ct->what[o] = encoding_fread(ct->enc_vt, fp)) == NULL)
            goto error;
    return ct;
error:
    fprintf(stderr, "%s: %s: reading ciphertext failed\n",
            errorstr, __func__);
    free(ct);
    return NULL;
}

static mife_sk_t *
mife_sk(const mife_t *mife)
{
    mife_sk_t *sk;
    sk = xcalloc(1, sizeof sk[0]);
    sk->mmap = mife->mmap;
    sk->circ = mife->op->circ;
    sk->enc_vt = mife->enc_vt;
    sk->pp_vt = mife->pp_vt;
    sk->sp_vt = mife->sp_vt;
    sk->sp = mife->sp;
    sk->pp = mife->pp;
    sk->const_alphas = mife->const_alphas;
    sk->deg_max = mife->deg_max;
    sk->local = false;
    return sk;
}

static void
mife_sk_free(mife_sk_t *sk)
{
    if (sk == NULL)
        return;
    if (sk->local) {
        if (sk->pp)
            public_params_free(sk->pp_vt, sk->pp);
        if (sk->sp)
            secret_params_free(sk->sp_vt, sk->sp);
        if (sk->const_alphas)
            mpz_vect_free(sk->const_alphas, acirc_nconsts(sk->circ));
        if (sk->deg_max)
            free(sk->deg_max);
    }
    free(sk);
}

static int
mife_sk_fwrite(const mife_sk_t *sk, FILE *fp)
{
    if (public_params_fwrite(sk->pp_vt, sk->pp, fp) == ERR) goto error;
    if (secret_params_fwrite(sk->sp_vt, sk->sp, fp) == ERR) goto error;
    if (acirc_nconsts(sk->circ))
        for (size_t o = 0; o < acirc_nconsts(sk->circ); ++o)
            if (mpz_fwrite(sk->const_alphas[o], fp) == ERR) goto error;
    for (size_t i = 0; i < acirc_nslots(sk->circ); ++i)
        if (size_t_fwrite(sk->deg_max[i], fp) == ERR) goto error;
    return OK;
error:
    fprintf(stderr, "%s: %s: writing mife secret key failed\n",
            errorstr, __func__);
    return ERR;
}

static mife_sk_t *
mife_sk_fread(const mmap_vtable *mmap, const acirc_t *circ, FILE *fp)
{
    mife_sk_t *sk;

    sk = xcalloc(1, sizeof sk[0]);
    sk->local = true;
    sk->mmap = mmap;
    sk->circ = circ;
    sk->enc_vt = get_encoding_vtable(mmap);
    sk->pp_vt = get_pp_vtable(mmap);
    sk->sp_vt = get_sp_vtable(mmap);
    if ((sk->pp = public_params_fread(sk->pp_vt, circ, fp)) == NULL) goto error;
    if ((sk->sp = secret_params_fread(sk->sp_vt, circ, fp)) == NULL) goto error;
    if (acirc_nconsts(circ)) {
        sk->const_alphas = xcalloc(acirc_nconsts(circ), sizeof sk->const_alphas[0]);
        for (size_t o = 0; o < acirc_nconsts(circ); ++o)
            if (mpz_fread(&sk->const_alphas[o], fp) == ERR) goto error;
    }
    sk->deg_max = xcalloc(acirc_nslots(circ), sizeof sk->deg_max[0]);
    for (size_t i = 0; i < acirc_nslots(circ); ++i)
        if (size_t_fread(&sk->deg_max[i], fp) == ERR) goto error;
    return sk;
error:
    fprintf(stderr, "%s: %s: reading mife secret key failed\n",
            errorstr, __func__);
    mife_sk_free(sk);
    return NULL;
}

static mife_ek_t *
mife_ek(const mife_t *mife)
{
    mife_ek_t *ek;
    ek = xcalloc(1, sizeof ek[0]);
    ek->mmap = mife->mmap;
    ek->circ = mife->op->circ;
    ek->enc_vt = mife->enc_vt;
    ek->pp_vt = mife->pp_vt;
    ek->pp = mife->pp;
    ek->Chatstar = mife->Chatstar;
    ek->zhat = mife->zhat;
    ek->npowers = mife->npowers;
    ek->uhat = mife->uhat;
    ek->constants = mife->constants;
    ek->local = false;
    return ek;
}

static void
mife_ek_free(mife_ek_t *ek)
{
    if (ek == NULL)
        return;
    if (ek->local) {
        if (ek->pp)
            public_params_free(ek->pp_vt, ek->pp);
        if (ek->Chatstar)
            encoding_free(ek->enc_vt, ek->Chatstar);
        if (ek->constants)
            mife_ct_free(ek->constants);
        if (ek->zhat)
            encoding_free(ek->enc_vt, ek->zhat);
        if (ek->uhat) {
            for (size_t i = 0; i < acirc_nslots(ek->circ); ++i) {
                if (ek->uhat[i]) {
                    for (size_t p = 0; p < ek->npowers; ++p)
                        if (ek->uhat[i][p])
                            encoding_free(ek->enc_vt, ek->uhat[i][p]);
                    free(ek->uhat[i]);
                }
            }
            free(ek->uhat);
        }
    }
    free(ek);
}

static int
mife_ek_fwrite(const mife_ek_t *ek, FILE *fp)
{
    public_params_fwrite(ek->pp_vt, ek->pp, fp);
    if (ek->constants) {
        bool_fwrite(true, fp);
        mife_ct_fwrite(ek->constants, fp);
    } else {
        bool_fwrite(false, fp);
        encoding_fwrite(ek->enc_vt, ek->Chatstar, fp);
    }
    encoding_fwrite(ek->enc_vt, ek->zhat, fp);
    size_t_fwrite(ek->npowers, fp);
    for (size_t i = 0; i < acirc_nslots(ek->circ); ++i)
        for (size_t p = 0; p < ek->npowers; ++p)
            encoding_fwrite(ek->enc_vt, ek->uhat[i][p], fp);
    return OK;
}

static mife_ek_t *
mife_ek_fread(const mmap_vtable *mmap, const acirc_t *circ, FILE *fp)
{
    mife_ek_t *ek;
    bool has_consts;

    ek = xcalloc(1, sizeof ek[0]);
    ek->local = true;
    ek->mmap = mmap;
    ek->circ = circ;
    ek->enc_vt = get_encoding_vtable(mmap);
    ek->pp_vt = get_pp_vtable(mmap);
    if ((ek->pp = public_params_fread(ek->pp_vt, circ, fp)) == NULL) goto error;
    if (bool_fread(&has_consts, fp) == ERR) goto error;
    if (has_consts) {
        if ((ek->constants = mife_ct_fread(ek->mmap, ek, fp)) == NULL) goto error;
    } else {
        if ((ek->Chatstar = encoding_fread(ek->enc_vt, fp)) == NULL) goto error;
    }
    if ((ek->zhat = encoding_fread(ek->enc_vt, fp)) == NULL) goto error;
    if (size_t_fread(&ek->npowers, fp) == ERR) goto error;
    ek->uhat = xcalloc(acirc_nslots(circ), sizeof ek->uhat[0]);
    for (size_t i = 0; i < acirc_nslots(circ); ++i) {
        ek->uhat[i] = xcalloc(ek->npowers, sizeof ek->uhat[i][0]);
        for (size_t p = 0; p < ek->npowers; ++p)
            if ((ek->uhat[i][p] = encoding_fread(ek->enc_vt, fp)) == NULL)
                goto error;
    }
    return ek;
error:
    mife_ek_free(ek);
    return NULL;
}

typedef struct {
    const encoding_vtable *vt;
    encoding *enc;
    mpz_t *inps;
    size_t nslots;
    index_set *ix;
    const secret_params *sp;
    pthread_mutex_t *lock;
    size_t *count;
    size_t total;
} encode_args_t;

size_t
mife_num_encodings_setup(const acirc_t *circ, size_t npowers)
{
    const size_t nconsts = acirc_nconsts(circ) ? acirc_symlen(circ, acirc_nsymbols(circ)) : 1;
    return 1 + acirc_nslots(circ) * npowers + nconsts;
}

size_t
mife_num_encodings_encrypt(const acirc_t *circ, size_t slot)
{
    return acirc_symlen(circ, slot) + acirc_noutputs(circ);
}

static int
populate_circ_degrees(const acirc_t *circ, size_t *maxdegs)
{
    const bool has_consts = acirc_nconsts(circ) > 0;
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        if ((maxdegs[i] = acirc_max_var_degree(circ, i)) == 0) return ERR;
    if (has_consts)
        if ((maxdegs[acirc_nsymbols(circ)] = acirc_max_const_degree(circ)) == 0) return ERR;
    return OK;
}

static int
populate_circ_input(const acirc_t *circ, size_t slot, mpz_t **inputs,
                    mpz_t **consts, const mpz_t *alphas)
{
    size_t idx = 0;
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
        for (size_t j = 0; j < acirc_symlen(circ, i); ++j) {
            if (i == slot)
                mpz_set   (*inputs[idx + j], alphas[j]);
            else
                mpz_set_ui(*inputs[idx + j], 1);
        }
        idx += acirc_symlen(circ, i);
    }
    for (size_t i = 0; i < acirc_nconsts(circ); ++i) {
        if (slot == acirc_nsymbols(circ))
            mpz_set   (*consts[i], alphas[i]);
        else
            mpz_set_ui(*consts[i], 1);
    }
    return OK;
}

static void
encode_worker(void *wargs)
{
    encode_args_t *args = wargs;

    encode(args->vt, args->enc, args->inps, args->nslots, args->ix, args->sp, 0);
    if (g_verbose) {
        pthread_mutex_lock(args->lock);
        print_progress(++*args->count, args->total);
        pthread_mutex_unlock(args->lock);
    }
    mpz_vect_free(args->inps, args->nslots);
    index_set_free(args->ix);
    free(args);
}

static int
__encode(threadpool *pool, const encoding_vtable *vt, encoding *enc, mpz_t *inps,
         size_t nslots, index_set *ix, const secret_params *sp,
         pthread_mutex_t *lock, size_t *count, size_t total)
{
    encode_args_t *args;
    args = xcalloc(1, sizeof args[0]);
    args->vt = vt;
    args->enc = enc;
    args->inps = xcalloc(nslots, sizeof args->inps[0]);
    for (size_t i = 0; i < nslots; ++i) {
        mpz_init_set(args->inps[i], inps[i]);
    }
    args->nslots = nslots;
    args->ix = ix;
    args->sp = sp;
    args->lock = lock;
    args->count = count;
    args->total = total;
    threadpool_add_job(pool, encode_worker, args);
    return OK;

}

static void
mife_free(mife_t *mife)
{
    if (mife == NULL)
        return;
    if (mife->Chatstar)
        encoding_free(mife->enc_vt, mife->Chatstar);
    if (mife->zhat)
        encoding_free(mife->enc_vt, mife->zhat);
    if (mife->uhat) {
        for (size_t i = 0; i < acirc_nslots(mife->op->circ); ++i) {
            if (mife->uhat[i]) {
                for (size_t p = 0; p < mife->npowers; ++p) {
                    if (mife->uhat[i][p])
                        encoding_free(mife->enc_vt, mife->uhat[i][p]);
                }
            }
            free(mife->uhat[i]);
        }
        free(mife->uhat);
    }
    if (mife->const_alphas)
        mpz_vect_free(mife->const_alphas, acirc_nconsts(mife->op->circ));
    if (mife->constants)
        mife_ct_free(mife->constants);
    if (mife->pp)
        public_params_free(mife->pp_vt, mife->pp);
    if (mife->sp)
        secret_params_free(mife->sp_vt, mife->sp);
    if (mife->deg_max)
        free(mife->deg_max);
    free(mife);
}

static mife_t *
mife_setup(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    int result = ERR;
    mife_t *mife;
    const acirc_t *circ = op->circ;
    threadpool *pool = threadpool_create(nthreads);
    pthread_mutex_t lock;
    size_t count = 0;
    size_t total = mife_num_encodings_setup(circ, op->npowers);
    index_set *ix;
    mpz_t *moduli;
    const size_t nslots = acirc_nslots(circ);
    mpz_t *inps;

    inps = mpz_vect_new(1 + nslots);

    mife = xcalloc(1, sizeof mife[0]);
    mife->mmap = mmap;
    mife->op = op;
    mife->enc_vt = get_encoding_vtable(mmap);
    mife->pp_vt = get_pp_vtable(mmap);
    mife->sp_vt = get_sp_vtable(mmap);
    if ((mife->sp = secret_params_new(mife->sp_vt, op, circ, secparam, kappa,
                                      nthreads, rng)) == NULL)
        goto cleanup;
    if ((mife->pp = public_params_new(mife->pp_vt, mife->sp_vt, mife->sp, op)) == NULL)
        goto cleanup;
    mife->npowers = op->npowers;
    mife->zhat = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
    mife->uhat = xcalloc(nslots, sizeof mife->uhat[0]);
    mife->deg_max = xcalloc(nslots, sizeof mife->deg_max[0]);
    if (populate_circ_degrees(circ, mife->deg_max) == ERR)
        goto cleanup;

    moduli = mmap->sk->plaintext_fields(mife->sp->sk);
    pthread_mutex_init(&lock, NULL);

    if (g_verbose)
        print_progress(count, total);

    {
        ix = index_set_new(mife_params_nzs(circ));
        if (mpz_cmp_ui(moduli[0], 2) == 0)
            mpz_set_ui(inps[0], 1);
        else
            mpz_randomm_inv(inps[0], rng, moduli[0]);
        for (size_t i = 0; i < nslots; ++i) {
            mpz_set_ui(inps[1 + i], 1);
            IX_W(ix, circ, i) = 1;
        }
        IX_Z(ix) = 1;
        /* Encode \hat z = [δ, 1, ..., 1] */
        __encode(pool, mife->enc_vt, mife->zhat, inps, 1 + nslots,
                 ix, mife->sp, &lock, &count, total);
    }
    for (size_t i = 0; i < 1 + nslots; ++i)
        mpz_set_ui(inps[i], 1);
    for (size_t i = 0; i < nslots; ++i) {
        mife->uhat[i] = xcalloc(mife->npowers, sizeof mife->uhat[i][0]);
        for (size_t p = 0; p < mife->npowers; ++p) {
            ix = index_set_new(mife_params_nzs(circ));
            mife->uhat[i][p] = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
            IX_X(ix, circ, i) = 1 << p;
            /* Encode \hat u_i,p = [1, ..., 1] */
            __encode(pool, mife->enc_vt, mife->uhat[i][p], inps, 1 + nslots,
                     ix, mife->sp, &lock, &count, total);
        }
    }
    if (acirc_nconsts(circ)) {
        /* Encrypt constants as part of setup */
        const size_t nconsts = acirc_nconsts(circ);
        mife_encrypt_cache_t cache = {
            .pool = pool,
            .lock = &lock,
            .count = &count,
            .total = total,
        };
        long consts[nconsts];
        mife_sk_t *sk = mife_sk(mife);
        for (size_t i = 0; i < nconsts; ++i)
            consts[i] = acirc_const(circ, i);
        mife->const_alphas = xcalloc(nconsts, sizeof mife->const_alphas[0]);
        mife->constants = mife_cmr_encrypt(sk, nslots - 1, consts, nconsts,
                                           nthreads, rng, &cache,
                                           mife->const_alphas);
        if (mife->constants == NULL) {
            fprintf(stderr, "%s: %s: unable to encrypt constants\n",
                    errorstr, __func__);
            goto cleanup;
        }
        mife_sk_free(sk);
        mife->Chatstar = NULL;
    } else {
        /* No constants, so encode \hat C* as normal */
        ix = index_set_new(mife_params_nzs(circ));
        mife->const_alphas = NULL;
        mife->constants = NULL;
        mife->Chatstar = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
        mpz_set_ui(inps[0], 0);
        for (size_t i = 0; i < nslots; ++i) {
            mpz_set_ui(inps[1 + i], 1);
            IX_X(ix, circ, i) = mife->deg_max[i];
        }
        IX_Z(ix) = 1;
        /* Encode \hat C* = [0, 1, ..., 1] */
        __encode(pool, mife->enc_vt, mife->Chatstar, inps, 1 + nslots,
                 ix, mife->sp, &lock, &count, total);
    }

    result = OK;
cleanup:
    mpz_vect_free(inps, 1 + nslots);
    threadpool_destroy(pool);
    pthread_mutex_destroy(&lock);
    if (result == OK)
        return mife;
    else {
        mife_free(mife);
        return NULL;
    }
}

mife_ct_t *
mife_cmr_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
                 size_t ninputs, size_t nthreads, aes_randstate_t rng,
                 mife_encrypt_cache_t *cache, mpz_t *_alphas)
{
    mife_ct_t *ct;
    double start, end, _start, _end;
    const acirc_t *circ = sk->circ;
    const size_t nconsts = acirc_nconsts(circ);
    const size_t nslots = acirc_nslots(circ);
    const size_t has_consts = nconsts ? 1 : 0;
    const size_t noutputs = acirc_noutputs(circ);
    const mpz_t *moduli = sk->mmap->sk->plaintext_fields(sk->sp->sk);
    index_set *ix = index_set_new(mife_params_nzs(sk->circ));
    mpz_t *slots;
    mpz_t *alphas;

    if (ninputs != acirc_symlen(circ, slot)) {
        fprintf(stderr, "%s: %s: invalid input length (%lu ≠ %lu)\n",
                errorstr, __func__, ninputs, acirc_symlen(circ, slot));
        return NULL;
    }

    start = current_time();
    _start = current_time();

    ct = xcalloc(1, sizeof ct[0]);
    ct->enc_vt = sk->enc_vt;
    ct->circ = circ;
    ct->slot = slot;
    ct->xhat = xcalloc(ninputs, sizeof ct->xhat[0]);
    for (size_t j = 0; j < ninputs; ++j)
        ct->xhat[j] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
    ct->what = xcalloc(noutputs, sizeof ct->what[0]);
    for (size_t o = 0; o < noutputs; ++o)
        ct->what[o] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);

    slots = mpz_vect_new(1 + nslots);
    alphas = _alphas ? _alphas : mpz_vect_new(ninputs);
    for (size_t j = 0; j < ninputs; ++j)
        mpz_randomm_inv(alphas[j], rng, moduli[1 + slot]);

    _end = current_time();
    if (g_verbose && !cache)
        fprintf(stderr, "  Initialize: %.2fs\n", _end - _start);

    threadpool *pool;
    pthread_mutex_t *lock;
    size_t *count, total;

    if (cache) {
        pool = cache->pool;
        lock = cache->lock;
        count = cache->count;
        total = cache->total;
    } else {
        pool = threadpool_create(nthreads);
        lock = xcalloc(1, sizeof lock[0]);
        pthread_mutex_init(lock, NULL);
        count = xcalloc(1, sizeof count[0]);
        total = mife_num_encodings_encrypt(circ, slot);
    }

    _start = current_time();

    if (g_verbose && !cache)
        print_progress(*count, total);

    /* Encode \hat xⱼ */
    index_set_clear(ix);
    IX_X(ix, circ, slot) = 1;
    for (size_t i = 0; i < nslots; ++i)
        mpz_set_ui(slots[1 + i], 1);
    for (size_t j = 0; j < ninputs; ++j) {
        mpz_set_ui(slots[0],        inputs[j]);
        mpz_set   (slots[1 + slot], alphas[j]);
        /* Encode \hat xⱼ := [xⱼ, 1, ..., 1, αⱼ, 1, ..., 1] */
        __encode(pool, sk->enc_vt, ct->xhat[j], slots, 1 + nslots,
                 index_set_copy(ix), sk->sp, lock, count, total);
    }
    /* Encode \hat wₒ */
    if (!_alphas) {
        /* If `_alphas` is given, then we don't encode wₒ, because these will
         * be multiplied into the wₒ's of the first MIFE slot */
        mpz_t **cs, **const_cs = NULL;
        mpz_t **circ_inputs, **consts;
        const size_t ntotal = acirc_ninputs(circ) + acirc_nconsts(circ);

        circ_inputs = xcalloc(ntotal, sizeof circ_inputs[0]);
        for (size_t i = 0; i < ntotal; ++i)
            circ_inputs[i] = mpz_vect_new(1);
        consts = xcalloc(nconsts, sizeof consts[0]);
        for (size_t i = 0; i < nconsts; ++i)
            consts[i] = mpz_vect_new(1);

        populate_circ_input(circ, slot, circ_inputs, consts, (const mpz_t *) alphas);
        cs = acirc_eval_mpz(circ, circ_inputs, consts, moduli[1 + slot]);
        if (slot == 0 && has_consts) {
            populate_circ_input(circ, nslots - 1, circ_inputs, consts, (const mpz_t *) sk->const_alphas);
            const_cs = acirc_eval_mpz(circ, circ_inputs, consts, moduli[nslots]);
        }

        index_set_clear(ix);
        IX_W(ix, circ, slot) = 1;
        if (slot == 0 && has_consts) {
            for (size_t i = 0; i < nslots; ++i)
                IX_X(ix, circ, i) = sk->deg_max[i];
            IX_W(ix, circ, nslots - 1) = 1;
            IX_Z(ix) = 1;
        }
        mpz_set_ui(slots[0], 0);
        for (size_t o = 0; o < noutputs; ++o) {
            mpz_set(slots[1 + slot], *cs[o]);
            mpz_vect_free(cs[o], 1);
            if (slot == 0 && has_consts) {
                mpz_set(slots[nslots], *const_cs[o]);
                mpz_vect_free(const_cs[o], 1);
            }
            /* Encode \hat wₒ = [0, 1, ..., 1, C†ₒ, 1, ..., 1] */
            __encode(pool, sk->enc_vt, ct->what[o], slots, 1 + nslots,
                     index_set_copy(ix), sk->sp, lock, count, total);
        }
        free(cs);
        if (const_cs)
            free(const_cs);

        for (size_t i = 0; i < ntotal; ++i)
            mpz_vect_free(circ_inputs[i], 1);
        free(circ_inputs);
        for (size_t i = 0; i < nconsts; ++i)
            mpz_vect_free(consts[i], 1);
        free(consts);
        mpz_vect_free(alphas, ninputs);
    }

    if (!cache) {
        threadpool_destroy(pool);
        pthread_mutex_destroy(lock);
        free(lock);
        free(count);
    }

    _end = current_time();
    if (g_verbose && !cache)
        fprintf(stderr, "  Encode: %.2fs\n", _end - _start);

    index_set_free(ix);
    mpz_vect_free(slots, 1 + nslots);

    end = current_time();
    if (g_verbose && !cache)
        fprintf(stderr, "  Total: %.2fs\n", end - start);

    return ct;
}

static mife_ct_t *
mife_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
             size_t ninputs, size_t nthreads, aes_randstate_t rng)
{
    if (sk == NULL || slot >= acirc_nslots(sk->circ) || inputs == NULL) {
        fprintf(stderr, "%s: %s: invalid input\n", errorstr, __func__);
        return NULL;
    }
    return mife_cmr_encrypt(sk, slot, inputs, ninputs, nthreads, rng, NULL, NULL);
}

static void
_raise_encoding(const mife_ek_t *ek, encoding *x, encoding **us, size_t diff)
{
    while (diff > 0) {
        size_t p = 0;
        while (((size_t) 1 << (p+1)) <= diff && (p+1) < ek->npowers)
            p++;
        encoding_mul(ek->enc_vt, ek->pp_vt, x, x, us[p], ek->pp);
        diff -= (1 << p);
    }
}

static int
raise_encoding(const mife_ek_t *ek, encoding *x, const index_set *target)
{
    const acirc_t *circ = ek->circ;
    const bool has_consts = acirc_nconsts(circ) > 0;
    const size_t nslots = acirc_nslots(circ);
    index_set *ix;
    size_t diff;

    if ((ix = index_set_difference(target, ek->enc_vt->mmap_set(x))) == NULL)
        return ERR;
    for (size_t i = 0; i < acirc_nsymbols(circ); i++) {
        diff = IX_X(ix, circ, i);
        if (diff > 0)
            _raise_encoding(ek, x, ek->uhat[i], diff);
    }
    if (has_consts) {
        diff = IX_X(ix, circ, nslots - 1);
        if (diff > 0)
            _raise_encoding(ek, x, ek->uhat[nslots - 1], diff);
    }
    index_set_free(ix);
    return OK;
}

static int
raise_encodings(const mife_ek_t *ek, encoding *x, encoding *y)
{
    index_set *ix;
    int ret = ERR;
    if ((ix = index_set_union(ek->enc_vt->mmap_set(x),
                              ek->enc_vt->mmap_set(y))) == NULL)
        goto cleanup;
    if (raise_encoding(ek, x, ix) == ERR)
        goto cleanup;
    if (raise_encoding(ek, y, ix) == ERR)
        goto cleanup;
    ret = OK;
cleanup:
    if (ix)
        index_set_free(ix);
    return ret;
}

typedef struct {
    const acirc_t *circ;
    const mife_ct_t **cts;
    const mife_ek_t *ek;
    const obf_params_t *op;
    size_t *kappas;
    const char *dirname;
    bool dir_exists;
} decrypt_args_t;

static void *
copy_f(void *x, void *args_)
{
    const decrypt_args_t *args = args_;
    const mife_ek_t *ek = args->ek;
    encoding *out;

    out = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
    encoding_set(ek->enc_vt, out, x);
    return out;
}

static void *
input_f(size_t ref, size_t i, void *args_)
{
    (void) ref;
    const decrypt_args_t *args = args_;
    const size_t slot = circ_params_slot(args->circ, i);
    const size_t bit = circ_params_bit(args->circ, i);
    return copy_f(args->cts[slot]->xhat[bit], args_);
}

static void *
const_f(size_t ref, size_t i, long val, void *args_)
{
    (void) ref; (void) val;
    const decrypt_args_t *args = args_;
    const size_t bit = circ_params_bit(args->circ, acirc_ninputs(args->circ) + i);
    return copy_f(args->ek->constants->xhat[bit], args_);
}

static void *
eval_f(size_t ref, acirc_op op, size_t xref, const void *x_, size_t yref, const void *y_, void *args_)
{
    (void) ref; (void) xref; (void) yref;
    const decrypt_args_t *args = args_;
    const mife_ek_t *ek = args->ek;
    const encoding *x = x_;
    const encoding *y = y_;
    encoding *res;

    if (x == NULL || y == NULL)
        return NULL;

    res = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
    switch (op) {
    case ACIRC_OP_MUL:
        encoding_mul(ek->enc_vt, ek->pp_vt, res, x, y, ek->pp);
        break;
    case ACIRC_OP_ADD:
    case ACIRC_OP_SUB: {
        encoding *tmp_x, *tmp_y;
        tmp_x = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        tmp_y = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        encoding_set(ek->enc_vt, tmp_x, x);
        encoding_set(ek->enc_vt, tmp_y, y);
        if (!index_set_eq(ek->enc_vt->mmap_set(tmp_x), ek->enc_vt->mmap_set(tmp_y)))
            raise_encodings(ek, tmp_x, tmp_y);
        if (op == ACIRC_OP_ADD) {
            encoding_add(ek->enc_vt, ek->pp_vt, res, tmp_x, tmp_y, ek->pp);
        } else {
            encoding_sub(ek->enc_vt, ek->pp_vt, res, tmp_x, tmp_y, ek->pp);
        }
        encoding_free(ek->enc_vt, tmp_x);
        encoding_free(ek->enc_vt, tmp_y);
        break;
    }
    }
    return res;
}

static void *
output_f(size_t ref, size_t o, void *x, void *args_)
{
    (void) ref;
    const decrypt_args_t *args = args_;
    const mife_ek_t *ek = args->ek;
    const index_set *toplevel = ek->pp_vt->toplevel(ek->pp);
    const size_t nslots = acirc_nslots(ek->circ);
    encoding *out, *lhs, *rhs;
    long output = 1;

    out = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
    lhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
    rhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);

    /* Compute LHS */
    encoding_mul(ek->enc_vt, ek->pp_vt, lhs, x, ek->zhat, ek->pp);
    raise_encoding(ek, lhs, toplevel);
    if (!index_set_eq(ek->enc_vt->mmap_set(lhs), toplevel)) {
        fprintf(stderr, "%s: %s: lhs != toplevel\n", errorstr, __func__);
        index_set_print(ek->enc_vt->mmap_set(lhs));
        index_set_print(toplevel);
        goto cleanup;
    }
    /* Compute RHS */
    if (ek->Chatstar) {
        encoding_set(ek->enc_vt, rhs, ek->Chatstar);
        for (size_t i = 0; i < nslots; ++i)
            encoding_mul(ek->enc_vt, ek->pp_vt, rhs, rhs,
                         args->cts[i]->what[o], ek->pp);
    } else {
        encoding_set(ek->enc_vt, rhs, args->cts[0]->what[o]);
        for (size_t i = 1; i < nslots - 1; ++i)
            encoding_mul(ek->enc_vt, ek->pp_vt, rhs, rhs,
                         args->cts[i]->what[o], ek->pp);
    }
    if (!index_set_eq(ek->enc_vt->mmap_set(rhs), toplevel)) {
        fprintf(stderr, "%s: %s: rhs != toplevel\n", errorstr, __func__);
        index_set_print(ek->enc_vt->mmap_set(rhs));
        index_set_print(toplevel);
        goto cleanup;
    }
    encoding_sub(ek->enc_vt, ek->pp_vt, out, lhs, rhs, ek->pp);
    output = !encoding_is_zero(ek->enc_vt, ek->pp_vt, out, ek->pp);
    if (args->kappas)
        args->kappas[o] = encoding_get_degree(ek->enc_vt, out);

cleanup:
    encoding_free(ek->enc_vt, out);
    encoding_free(ek->enc_vt, lhs);
    encoding_free(ek->enc_vt, rhs);
    return (void *) output;
}

static void
free_f(void *x, void *args_)
{
    const decrypt_args_t *args = args_;
    if (x)
        encoding_free(args->ek->enc_vt, x);
}

static FILE *
open_ref_file(const char *dirname, size_t ref, const char *mode)
{
    char *fname = NULL;
    FILE *fp = NULL;

    if ((fname = makestr("%s/%lu", dirname, ref)) == NULL)
        goto error;
    if ((fp = fopen(fname, mode)) == NULL)
        goto error;
    free(fname);
    return fp;
error:
    if (fname)
        free(fname);
    if (fp)
        fclose(fp);
    return NULL;
}

static int
write_f(size_t ref, void *x, void *args_)
{
    if (x == NULL)
        return ACIRC_OK;

    decrypt_args_t *args = args_;
    int ret = ACIRC_ERR;
    FILE *fp = NULL;
    if (args->dirname == NULL)
        return OK;
    if (!args->dir_exists) {
        if (makedir(args->dirname) == ERR)
            goto cleanup;
        args->dir_exists = true;
    }
    if ((fp = open_ref_file(args->dirname, ref, "w")) == NULL)
        goto cleanup;
    if (encoding_fwrite(args->ek->enc_vt, x, fp) == ERR)
        goto cleanup;
    ret = ACIRC_OK;
cleanup:
    if (fp)
        fclose(fp);
    return ret;
}

static void *
read_f(size_t ref, void *args_)
{
    const decrypt_args_t *args = args_;
    void *rop = NULL;
    FILE *fp = NULL;
    if ((fp = open_ref_file(args->dirname, ref, "r")) == NULL)
        return NULL;
    rop = encoding_fread(args->ek->enc_vt, fp);
    fclose(fp);
    return rop;
}

int
mife_cmr_decrypt(const mife_ek_t *ek, long *rop, const mife_ct_t **cts,
                 size_t nthreads, size_t *kappa, bool save, bool load)
{
    const acirc_t *circ = ek->circ;
    char *dirname = NULL;
    int ret = ERR;

    if (ek == NULL || cts == NULL)
        return ERR;

    size_t *kappas = NULL;

    if (kappa)
        kappas = xcalloc(acirc_noutputs(circ), sizeof kappas[0]);

    {
        long *out;
        if (save)
            dirname = makestr("%s.encodings", acirc_fname(circ));
        decrypt_args_t args = {
            .circ = circ,
            .cts = cts,
            .ek = ek,
            .kappas = kappas,
            .dirname = dirname,
        };
        out = (long *) acirc_traverse((acirc_t *) circ, input_f, const_f,
                                      eval_f, output_f, free_f,
                                      save ? write_f : NULL,
                                      load ? read_f : NULL, &args, nthreads);
        if (out == NULL)
            goto cleanup;
        if (rop)
            for (size_t i = 0; i < acirc_noutputs(circ); ++i)
                rop[i] = out[i];
        free(out);
    }

    if (kappa) {
        size_t maxkappa = 0;
        for (size_t i = 0; i < acirc_noutputs(circ); i++) {
            if (kappas[i] > maxkappa)
                maxkappa = kappas[i];
        }
        *kappa = maxkappa;
    }
    ret = OK;
cleanup:
    if (dirname)
        free(dirname);
    if (kappa && kappas)
        free(kappas);
    return ret;
}

static int
mife_decrypt(const mife_ek_t *ek, long *rop, const mife_ct_t **cts,
             size_t nthreads, size_t *kappa)
{
    return mife_cmr_decrypt(ek, rop, cts, nthreads, kappa, false, false);
}

mife_vtable mife_cmr_vtable = {
    .mife_setup = mife_setup,
    .mife_free = mife_free,
    .mife_sk = mife_sk,
    .mife_sk_free = mife_sk_free,
    .mife_sk_fwrite = mife_sk_fwrite,
    .mife_sk_fread = mife_sk_fread,
    .mife_ek = mife_ek,
    .mife_ek_free = mife_ek_free,
    .mife_ek_fwrite = mife_ek_fwrite,
    .mife_ek_fread = mife_ek_fread,
    .mife_ct_free = mife_ct_free,
    .mife_ct_fwrite = mife_ct_fwrite,
    .mife_ct_fread = mife_ct_fread,
    .mife_encrypt = mife_encrypt,
    .mife_decrypt = mife_decrypt,
};
