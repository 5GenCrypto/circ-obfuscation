#include "mife.h"

#include "../index_set.h"
#include "mife_params.h"
#include "../vtables.h"
#include "../util.h"

#include <assert.h>
#include <string.h>

typedef struct mife_t {
    const mmap_vtable *mmap;
    const circ_params_t *cp;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    secret_params *sp;
    public_params *pp;
    size_t npowers;
    encoding *Chatstar;
    encoding **zhat;            /* [m] */
    encoding ***uhat;           /* [n][npowers] */
    mife_ciphertext_t *constants;
    mpz_t *const_alphas;
    long *deg_max;              /* [n] */
} mife_t;

typedef struct mife_sk_t {
    const mmap_vtable *mmap;
    const circ_params_t *cp;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    secret_params *sp;
    public_params *pp;
    mpz_t *const_alphas;
    long *deg_max;              /* [n] */
    bool local;
} mife_sk_t;

typedef struct mife_ek_t {
    const mmap_vtable *mmap;
    const circ_params_t *cp;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    public_params *pp;
    size_t npowers;
    encoding *Chatstar;
    encoding **zhat;            /* [m] */
    encoding ***uhat;           /* [n][npowers] */
    mife_ciphertext_t *constants;
    bool local;
} mife_ek_t;

typedef struct mife_ciphertext_t {
    const encoding_vtable *enc_vt;
    size_t slot;
    encoding **xhat;             /* [d_i] */
    encoding **what;             /* [m] */
} mife_ciphertext_t;

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

/* typedef struct { */
/*     const mmap_vtable *mmap; */
/*     acircref ref; */
/*     const acirc *c; */
/*     mife_ciphertext_t **cts; */
/*     const mife_ek_t *ek; */
/*     bool *mine; */
/*     int *ready; */
/*     void *cache; */
/*     ref_list *deps; */
/*     threadpool *pool; */
/*     int *rop; */
/*     size_t *kappas; */
/* } decrypt_args_t; */

static mife_ciphertext_t *
_mife_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
              size_t nthreads, aes_randstate_t rng, mife_encrypt_cache_t *cache,
              mpz_t *alphas, bool parallelize_circ_eval);

static int
populate_circ_degrees(const circ_params_t *cp, long **deg, long *deg_max)
{
    const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0;
    const size_t noutputs = cp->m;
    const acirc_t *circ = cp->circ;
    for (size_t i = 0; i < cp->n - has_consts; ++i) {
        long *tmp = acirc_var_degrees(circ, i);
        memcpy(deg[i], tmp, noutputs * sizeof deg[i][0]);
        for (size_t o = 0; o < noutputs; ++o) {
            if (deg[i][o] > deg_max[i])
                deg_max[i] = deg[i][o];
        }
        free(tmp);
    }
    if (has_consts) {
        for (size_t o = 0; o < noutputs; ++o)
            deg[cp->n - 1][o] = acirc_max_const_degree(circ);
        deg_max[cp->n - 1] = acirc_max_const_degree(circ);
    }
    return OK;
}

static int
populate_circ_input(const circ_params_t *cp, size_t slot, mpz_t **inputs,
                    mpz_t **consts, const mpz_t *alphas)
{
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t has_consts = nconsts ? 1 : 0;
    size_t idx = 0;
    for (size_t i = 0; i < cp->n - has_consts; ++i) {
        for (size_t j = 0; j < cp->ds[i]; ++j) {
            if (i == slot)
                mpz_set   (*inputs[idx + j], alphas[j]);
            else
                mpz_set_ui(*inputs[idx + j], 1);
        }
        idx += cp->ds[i];
    }
    for (size_t i = 0; i < nconsts; ++i) {
        if (has_consts && slot == cp->n - 1)
            mpz_set   (*consts[i], alphas[i]);
        else
            mpz_set_ui(*consts[i], 1);
    }
    return OK;
}

static mpz_t **
eval_circ(const circ_params_t *cp, size_t slot, mpz_t **inputs,
          mpz_t **consts, const mpz_t *moduli, mpz_t *_refs, size_t nthreads)
{
    (void) _refs; (void) nthreads;
    return acirc_eval_mpz(cp->circ, inputs, consts, moduli[1 + slot]);
    /* const size_t nrefs = acirc_nrefs(cp->circ); */
    /* mpz_t *refs; */

    /* if (_refs) { */
    /*     refs = _refs; */
    /* } else */
    /*     refs = my_calloc(nrefs, sizeof refs[0]); */
    /* circ_eval(cp->circ, inputs, consts, moduli[1 + slot], refs, nthreads); */
    /* for (size_t o = 0; o < cp->m; ++o) { */
    /*     mpz_set(outputs[o], refs[cp->circ->outputs.buf[o]]); */
    /* } */
    /* if (!_refs) { */
    /*     for (size_t i = 0; i < nrefs; ++i) */
    /*         mpz_clear(refs[i]); */
    /*     free(refs); */
    /* } */
    /* return OK; */
}

static void
encode_worker(void *wargs)
{
    encode_args_t *const args = wargs;

    encode(args->vt, args->enc, args->inps, args->nslots, args->ix, args->sp);
    if (g_verbose) {
        pthread_mutex_lock(args->lock);
        print_progress(++*args->count, args->total);
        pthread_mutex_unlock(args->lock);
    }
    mpz_vect_free(args->inps, args->nslots);
    index_set_free(args->ix);
    free(args);
}

static void
__encode(threadpool *pool, const encoding_vtable *vt, encoding *enc, mpz_t *inps,
         size_t nslots, index_set *ix, const secret_params *sp,
         pthread_mutex_t *lock, size_t *count, size_t total)
{
    encode_args_t *args = my_calloc(1, sizeof args[0]);
    args->vt = vt;
    args->enc = enc;
    args->inps = my_calloc(nslots, sizeof args->inps[0]);
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
}

void
mife_free(mife_t *mife)
{
    if (mife == NULL)
        return;
    if (mife->Chatstar)
        encoding_free(mife->enc_vt, mife->Chatstar);
    if (mife->zhat) {
        for (size_t o = 0; o < mife->cp->m; ++o)
            encoding_free(mife->enc_vt, mife->zhat[o]);
        free(mife->zhat);
    }
    if (mife->uhat) {
        for (size_t i = 0; i < mife->cp->n; ++i) {
            for (size_t p = 0; p < mife->npowers; ++p) {
                encoding_free(mife->enc_vt, mife->uhat[i][p]);
            }
            free(mife->uhat[i]);
        }
        free(mife->uhat);
    }
    if (mife->const_alphas)
        mpz_vect_free(mife->const_alphas, mife->cp->c);
    if (mife->constants)
        mife_ciphertext_free(mife->constants, mife->cp);
    if (mife->pp)
        public_params_free(mife->pp_vt, mife->pp);
    if (mife->sp)
        secret_params_free(mife->sp_vt, mife->sp);
    if (mife->deg_max)
        free(mife->deg_max);
    free(mife);
}

mife_t *
mife_setup(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t npowers, size_t nthreads, aes_randstate_t rng)
{
    int result = ERR;
    mife_t *mife;
    const circ_params_t *cp = &op->cp;
    const size_t has_consts = cp->c ? 1 : 0;
    const size_t noutputs = cp->m;
    long **deg;
    mpz_t *moduli = NULL;
    threadpool *pool = threadpool_create(nthreads);
    pthread_mutex_t lock;
    size_t count = 0;
    size_t total = mife_num_encodings_setup(cp, npowers);
    index_set *const ix = index_set_new(mife_params_nzs(cp));
    mpz_t inps[1 + cp->n];
    mpz_vect_init(inps, 1 + cp->n);

    mife = my_calloc(1, sizeof mife[0]);
    mife->mmap = mmap;
    mife->cp = cp;
    mife->enc_vt = get_encoding_vtable(mmap);
    mife->pp_vt = get_pp_vtable(mmap);
    mife->sp_vt = get_sp_vtable(mmap);
    mife->sp = secret_params_new(mife->sp_vt, op, secparam, kappa, nthreads, rng);
    if (mife->sp == NULL)
        goto cleanup;
    mife->pp = public_params_new(mife->pp_vt, mife->sp_vt, mife->sp);
    if (mife->pp == NULL)
        goto cleanup;
    mife->npowers = npowers;
    mife->zhat = my_calloc(noutputs, sizeof mife->zhat[0]);
    mife->uhat = my_calloc(cp->n, sizeof mife->uhat[0]);

    moduli = mpz_vect_create_of_fmpz(mmap->sk->plaintext_fields(mife->sp->sk),
                                     mmap->sk->nslots(mife->sp->sk));

    /* Compute input degrees */
    deg = my_calloc(cp->n, sizeof deg[0]);
    for (size_t i = 0; i < cp->n; ++i)
        deg[i] = my_calloc(noutputs, sizeof deg[i][0]);
    mife->deg_max = my_calloc(cp->n, sizeof mife->deg_max[0]);
    populate_circ_degrees(cp, deg, mife->deg_max);

    pthread_mutex_init(&lock, NULL);

    if (g_verbose)
        print_progress(count, total);

    for (size_t o = 0; o < noutputs; ++o) {
        mife->zhat[o] = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
        mpz_randomm_inv(inps[0], rng, moduli[0]);
        index_set_clear(ix);
        for (size_t i = 0; i < cp->n; ++i) {
            mpz_set_ui(inps[1 + i], 1);
            IX_W(ix, cp, i) = 1;
            IX_X(ix, cp, i) = mife->deg_max[i] - deg[i][o];
        }
        IX_Z(ix) = 1;
        /* Encode \hat z_o = [δ, 1, ..., 1] */
        __encode(pool, mife->enc_vt, mife->zhat[o], inps, 1 + cp->n,
                 index_set_copy(ix), mife->sp, &lock, &count, total);
    }
    for (size_t i = 0; i < 1 + cp->n; ++i) {
        mpz_set_ui(inps[i], 1);
    }
    for (size_t i = 0; i < cp->n; ++i) {
        mife->uhat[i] = my_calloc(mife->npowers, sizeof mife->uhat[i][0]);
        /* Encode \hat u_i,p */
        index_set_clear(ix);
        for (size_t p = 0; p < mife->npowers; ++p) {
            mife->uhat[i][p] = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
            IX_X(ix, cp, i) = 1 << p;
            /* Encode \hat u_i,p = [1, ..., 1] */
            __encode(pool, mife->enc_vt, mife->uhat[i][p], inps, 1 + cp->n,
                     index_set_copy(ix), mife->sp, &lock, &count, total);
        }
    }
    if (has_consts) {
        mife->const_alphas = my_calloc(cp->c, sizeof mife->const_alphas[0]);
        /* Encrypt constants as part of setup */
        mife_encrypt_cache_t cache = {
            .pool = pool,
            .lock = &lock,
            .count = &count,
            .total = total,
            .refs = NULL,
        };
        long consts[acirc_nconsts(cp->circ)];
        mife_sk_t *sk = mife_sk(mife);
        for (size_t i = 0; i < acirc_nconsts(cp->circ); ++i) {
            consts[i] = acirc_const(cp->circ, i);
        }
        mife->constants = _mife_encrypt(sk, cp->n - 1, consts, nthreads, rng,
                                        &cache, mife->const_alphas, false);
        if (mife->constants == NULL) {
            fprintf(stderr, "error: mife setup: unable to encrypt constants\n");
            goto cleanup;
        }
        mife_sk_free(sk);
        mife->Chatstar = NULL;
    } else {
        /* No constants, so encode \hat C* as normal */
        mife->const_alphas = NULL;
        mife->constants = NULL;
        mife->Chatstar = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
        index_set_clear(ix);
        mpz_set_ui(inps[0], 0);
        for (size_t i = 0; i < cp->n; ++i) {
            mpz_set_ui(inps[1 + i], 1);
            IX_X(ix, cp, i) = mife->deg_max[i];
        }
        IX_Z(ix) = 1;
        /* Encode \hat C* = [0, 1, ..., 1] */
        __encode(pool, mife->enc_vt, mife->Chatstar, inps, 1 + cp->n,
                 index_set_copy(ix), mife->sp, &lock, &count, total);
    }

    result = OK;
cleanup:
    index_set_free(ix);
    mpz_vect_clear(inps, 1 + cp->n);
    for (size_t i = 0; i < cp->n; ++i)
        free(deg[i]);
    free(deg);
    mpz_vect_free(moduli, mmap->sk->nslots(mife->sp->sk));
    threadpool_destroy(pool);
    pthread_mutex_destroy(&lock);
    if (result == OK)
        return mife;
    else {
        mife_free(mife);
        return NULL;
    }
}

mife_sk_t *
mife_sk(const mife_t *mife)
{
    mife_sk_t *sk;
    sk = my_calloc(1, sizeof sk[0]);
    sk->mmap = mife->mmap;
    sk->cp = mife->cp;
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

void
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
            mpz_vect_free(sk->const_alphas, sk->cp->c);
        if (sk->deg_max)
            free(sk->deg_max);
    }
    free(sk);
}

int
mife_sk_fwrite(const mife_sk_t *sk, FILE *fp)
{
    public_params_fwrite(sk->pp_vt, sk->pp, fp);
    secret_params_fwrite(sk->sp_vt, sk->sp, fp);
    if (sk->cp->c)
        for (size_t o = 0; o < sk->cp->c; ++o)
            if (mpz_fwrite(sk->const_alphas[o], fp) == ERR)
                goto error;
    for (size_t i = 0; i < sk->cp->n; ++i)
        if (size_t_fwrite(sk->deg_max[i], fp) == ERR)
            goto error;
    return OK;
error:
    fprintf(stderr, "error: writing mife secret key failed\n");
    return ERR;
}

mife_sk_t *
mife_sk_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    const circ_params_t *cp = &op->cp;
    double start, end;
    mife_sk_t *sk;

    sk = my_calloc(1, sizeof sk[0]);
    sk->local = true;
    sk->mmap = mmap;
    sk->cp = cp;
    sk->enc_vt = get_encoding_vtable(mmap);
    sk->pp_vt = get_pp_vtable(mmap);
    sk->sp_vt = get_sp_vtable(mmap);
    start = current_time();
    if ((sk->pp = public_params_fread(sk->pp_vt, op, fp)) == NULL)
        goto error;
    end = current_time();
    if (g_verbose)
        fprintf(stderr, "  Reading pp from disk: %.2fs\n", end - start);
    start = current_time();
    if ((sk->sp = secret_params_fread(sk->sp_vt, cp, fp)) == NULL)
        goto error;
    end = current_time();
    if (g_verbose)
        fprintf(stderr, "  Reading sp from disk: %.2fs\n", end - start);
    if (sk->cp->c) {
        sk->const_alphas = my_calloc(sk->cp->c, sizeof sk->const_alphas[0]);
        for (size_t o = 0; o < sk->cp->c; ++o)
            if (mpz_fread(&sk->const_alphas[o], fp) == ERR)
                goto error;
    }
    sk->deg_max = my_calloc(sk->cp->n, sizeof sk->deg_max[0]);
    for (size_t i = 0; i < sk->cp->n; ++i)
        if (size_t_fread((size_t *) &sk->deg_max[i], fp) == ERR)
            goto error;
    return sk;
error:
    fprintf(stderr, "error: reading mife secret key failed\n");
    mife_sk_free(sk);
    return NULL;
}

mife_ek_t *
mife_ek(const mife_t *mife)
{
    mife_ek_t *ek;
    ek = my_calloc(1, sizeof ek[0]);
    ek->mmap = mife->mmap;
    ek->cp = mife->cp;
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

void
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
            mife_ciphertext_free(ek->constants, ek->cp);
        if (ek->zhat) {
            for (size_t o = 0; o < ek->cp->m; ++o)
                encoding_free(ek->enc_vt, ek->zhat[o]);
            free(ek->zhat);
        }
        if (ek->uhat) {
            for (size_t i = 0; i < ek->cp->n; ++i) {
                for (size_t p = 0; p < ek->npowers; ++p)
                    encoding_free(ek->enc_vt, ek->uhat[i][p]);
                free(ek->uhat[i]);
            }
            free(ek->uhat);
        }
    }
    free(ek);
}

int
mife_ek_fwrite(const mife_ek_t *ek, FILE *fp)
{
    public_params_fwrite(ek->pp_vt, ek->pp, fp);
    if (ek->constants) {
        bool_fwrite(true, fp);
        mife_ciphertext_fwrite(ek->constants, ek->cp, fp);
    } else {
        bool_fwrite(false, fp);
        encoding_fwrite(ek->enc_vt, ek->Chatstar, fp);
    }
    for (size_t o = 0; o < ek->cp->m; ++o)
        encoding_fwrite(ek->enc_vt, ek->zhat[o], fp);
    size_t_fwrite(ek->npowers, fp);
    for (size_t i = 0; i < ek->cp->n; ++i)
        for (size_t p = 0; p < ek->npowers; ++p)
            encoding_fwrite(ek->enc_vt, ek->uhat[i][p], fp);
    return OK;
}

mife_ek_t *
mife_ek_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    const circ_params_t *cp = &op->cp;
    mife_ek_t *ek;
    bool has_consts;

    ek = my_calloc(1, sizeof ek[0]);
    ek->local = true;
    ek->mmap = mmap;
    ek->cp = cp;
    ek->enc_vt = get_encoding_vtable(mmap);
    ek->pp_vt = get_pp_vtable(mmap);
    ek->pp = public_params_fread(ek->pp_vt, op, fp);
    bool_fread(&has_consts, fp);
    if (has_consts) {
        if ((ek->constants = mife_ciphertext_fread(ek->mmap, ek->cp, fp)) == NULL)
            goto error;
    } else {
        if ((ek->Chatstar = encoding_fread(ek->enc_vt, fp)) == NULL)
            goto error;
    }
    ek->zhat = my_calloc(cp->m, sizeof ek->zhat[0]);
    for (size_t o = 0; o < cp->m; ++o)
        ek->zhat[o] = encoding_fread(ek->enc_vt, fp);
    size_t_fread(&ek->npowers, fp);
    ek->uhat = my_calloc(ek->cp->n, sizeof ek->uhat[0]);
    for (size_t i = 0; i < ek->cp->n; ++i) {
        ek->uhat[i] = my_calloc(ek->npowers, sizeof ek->uhat[i][0]);
        for (size_t p = 0; p < ek->npowers; ++p)
            ek->uhat[i][p] = encoding_fread(ek->enc_vt, fp);
    }
    return ek;
error:
    mife_ek_free(ek);
    return NULL;
}

void
mife_ciphertext_free(mife_ciphertext_t *ct, const circ_params_t *cp)
{
    if (ct == NULL)
        return;
    const size_t ninputs = cp->ds[ct->slot];
    for (size_t j = 0; j < ninputs; ++j)
        encoding_free(ct->enc_vt, ct->xhat[j]);
    free(ct->xhat);
    for (size_t o = 0; o < cp->m; ++o)
        encoding_free(ct->enc_vt, ct->what[o]);
    free(ct->what);
    free(ct);
}

int
mife_ciphertext_fwrite(const mife_ciphertext_t *ct, const circ_params_t *cp,
                       FILE *fp)
{
    const size_t ninputs = cp->ds[ct->slot];
    if (size_t_fwrite(ct->slot, fp) == ERR)
        return ERR;
    for (size_t j = 0; j < ninputs; ++j)
        if (encoding_fwrite(ct->enc_vt, ct->xhat[j], fp) == ERR)
            return ERR;
    for (size_t o = 0; o < cp->m; ++o)
        if (encoding_fwrite(ct->enc_vt, ct->what[o], fp) == ERR)
            return ERR;
    return OK;
}

mife_ciphertext_t *
mife_ciphertext_fread(const mmap_vtable *mmap, const circ_params_t *cp, FILE *fp)
{
    mife_ciphertext_t *ct;
    size_t ninputs;

    ct = my_calloc(1, sizeof ct[0]);
    ct->enc_vt = get_encoding_vtable(mmap);
    if (size_t_fread(&ct->slot, fp) == ERR)
        goto error;
    if (ct->slot >= cp->n) {
        fprintf(stderr, "error: slot number > number of slots\n");
        goto error;
    }
    ninputs = cp->ds[ct->slot];
    ct->xhat = my_calloc(ninputs, sizeof ct->xhat[0]);
    for (size_t j = 0; j < ninputs; ++j) {
        if ((ct->xhat[j] = encoding_fread(ct->enc_vt, fp)) == NULL)
            goto error;
    }
    ct->what = my_calloc(cp->m, sizeof ct->what[0]);
    for (size_t o = 0; o < cp->m; ++o)
        if ((ct->what[o] = encoding_fread(ct->enc_vt, fp)) == NULL)
            goto error;
    return ct;
error:
    fprintf(stderr, "error: reading ciphertext failed\n");
    free(ct);
    return NULL;
}

static mife_ciphertext_t *
_mife_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
              size_t nthreads, aes_randstate_t rng, mife_encrypt_cache_t *cache,
              mpz_t *_alphas, bool parallelize_circ_eval)
{
    mife_ciphertext_t *ct;
    double start, end, _start, _end;
    const circ_params_t *cp = sk->cp;
    mpz_t *refs = cache ? cache->refs : NULL;

    if (g_verbose && !cache)
        fprintf(stderr, "  Encrypting...\n");

    start = current_time();
    _start = current_time();

    const size_t ninputs = cp->ds[slot];
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t has_consts = nconsts ? 1 : 0;
    const size_t noutputs = cp->m;
    mpz_t *const moduli =
        mpz_vect_create_of_fmpz(sk->mmap->sk->plaintext_fields(sk->sp->sk),
                                sk->mmap->sk->nslots(sk->sp->sk));
    index_set *const ix = index_set_new(mife_params_nzs(cp));
    mpz_t *slots;
    mpz_t *alphas;

    ct = my_calloc(1, sizeof ct[0]);
    ct->enc_vt = sk->enc_vt;
    ct->slot = slot;
    ct->xhat = my_calloc(ninputs, sizeof ct->xhat[0]);
    for (size_t j = 0; j < ninputs; ++j)
        ct->xhat[j] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
    ct->what = my_calloc(noutputs, sizeof ct->what[0]);
    for (size_t o = 0; o < noutputs; ++o)
        ct->what[o] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);

    slots = mpz_vect_new(1 + cp->n);
    alphas = _alphas ? _alphas : mpz_vect_new(ninputs);
    for (size_t j = 0; j < ninputs; ++j)
        mpz_randomm_inv(alphas[j], rng, moduli[1 + slot]);

    _end = current_time();
    if (g_verbose && !cache)
        fprintf(stderr, "    Initialize: %.2fs\n", _end - _start);

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
        lock = my_calloc(1, sizeof lock[0]);
        pthread_mutex_init(lock, NULL);
        count = my_calloc(1, sizeof count[0]);
        total = mife_num_encodings_encrypt(cp, slot);
    }

    _start = current_time();

    if (g_verbose && !cache)
        print_progress(*count, total);

    /* Encode \hat xⱼ */
    index_set_clear(ix);
    IX_X(ix, cp, slot) = 1;
    for (size_t i = 0; i < cp->n; ++i)
        mpz_set_ui(slots[1 + i], 1);
    for (size_t j = 0; j < ninputs; ++j) {
        mpz_set_ui(slots[0],        inputs[j]);
        mpz_set   (slots[1 + slot], alphas[j]);
        /* Encode \hat xⱼ := [xⱼ, 1, ..., 1, αⱼ, 1, ..., 1] */
        __encode(pool, sk->enc_vt, ct->xhat[j], slots, 1 + cp->n,
                 index_set_copy(ix), sk->sp, lock, count, total);
    }
    /* Encode \hat wₒ */
    if (!_alphas) {
        /* If `_alphas` is given, then we don't encode wₒ, because these will
         * be multiplied into the wₒ's of the first MIFE slot */
        mpz_t **cs, **const_cs = NULL;
        mpz_t **circ_inputs, **consts;
        size_t _nthreads = parallelize_circ_eval ? nthreads : 0;

        circ_inputs = calloc(circ_params_ninputs(cp), sizeof circ_inputs[0]);
        for (size_t i = 0; i < circ_params_ninputs(cp); ++i) {
            circ_inputs[i] = mpz_vect_new(1);
        }
        consts = calloc(nconsts, sizeof consts[0]);
        for (size_t i = 0; i < nconsts; ++i) {
            consts[i] = mpz_vect_new(1);
        }

        populate_circ_input(cp, slot, circ_inputs, consts, alphas);
        assert(refs == NULL);
        cs = eval_circ(cp, slot, circ_inputs, consts, moduli, refs, _nthreads);
        if (slot == 0 && has_consts) {
            populate_circ_input(cp, cp->n - 1, circ_inputs, consts, sk->const_alphas);
            const_cs = eval_circ(cp, cp->n - 1, circ_inputs, consts, moduli, refs, _nthreads);
        }

        index_set_clear(ix);
        IX_W(ix, cp, slot) = 1;
        if (slot == 0 && has_consts) {
            for (size_t i = 0; i < cp->n; ++i)
                IX_X(ix, cp, i) = sk->deg_max[i];
            IX_W(ix, cp, cp->n - 1) = 1;
            IX_Z(ix) = 1;
        }
        mpz_set_ui(slots[0], 0);
        for (size_t o = 0; o < noutputs; ++o) {
            mpz_set(slots[1 + slot], *cs[o]);
            mpz_clear(*cs[o]);
            free(cs[o]);
            if (slot == 0 && has_consts) {
                mpz_set(slots[cp->n], *const_cs[o]);
                mpz_clear(*const_cs[o]);
                free(const_cs[o]);
            }
            /* Encode \hat wₒ = [0, 1, ..., 1, C†ₒ, 1, ..., 1] */
            __encode(pool, sk->enc_vt, ct->what[o], slots, 1 + cp->n,
                     index_set_copy(ix), sk->sp, lock, count, total);
        }
        free(cs);
        if (const_cs)
            free(const_cs);

        for (size_t i = 0; i < circ_params_ninputs(cp); ++i) {
            mpz_vect_free(circ_inputs[i], 1);
        }
        free(circ_inputs);
        for (size_t i = 0; i < nconsts; ++i) {
            mpz_vect_free(consts[i], 1);
        }
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
        fprintf(stderr, "    Encode: %.2fs\n", _end - _start);

    index_set_free(ix);
    mpz_vect_free(slots, 1 + cp->n);
    mpz_vect_free(moduli, sk->mmap->sk->nslots(sk->sp->sk));

    end = current_time();
    if (g_verbose && !cache)
        fprintf(stderr, "    Total: %.2fs\n", end - start);

    return ct;
}

mife_ciphertext_t *
mife_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
             size_t nthreads, mife_encrypt_cache_t *cache, aes_randstate_t rng,
             bool parallelize_circ_eval)
{
    if (sk == NULL || slot >= sk->cp->n || inputs == NULL) {
        fprintf(stderr, "error: mife encrypt: invalid input\n");
        return NULL;
    }
    return _mife_encrypt(sk, slot, inputs, nthreads, rng, cache, NULL,
                         parallelize_circ_eval);
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
    const circ_params_t *const cp = ek->cp;
    const size_t has_consts = cp->c ? 1 : 0;
    index_set *ix;
    size_t diff;

    ix = index_set_difference(target, ek->enc_vt->mmap_set(x));
    if (ix == NULL)
        return ERR;
    for (size_t i = 0; i < cp->n - has_consts; i++) {
        diff = IX_X(ix, cp, i);
        if (diff > 0)
            _raise_encoding(ek, x, ek->uhat[i], diff);
    }
    if (has_consts) {
        diff = IX_X(ix, cp, cp->n - 1);
        if (diff > 0)
            _raise_encoding(ek, x, ek->uhat[cp->n - 1], diff);
    }
    index_set_free(ix);
    return OK;
}

static int
raise_encodings(const mife_ek_t *ek, encoding *x, encoding *y)
{
    index_set *ix;
    int ret = ERR;
    ix = index_set_union(ek->enc_vt->mmap_set(x), ek->enc_vt->mmap_set(y));
    if (ix == NULL)
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
    circ_params_t *cp;
    mife_ciphertext_t **cts;
    mife_ek_t *ek;
} decrypt_args_t;

static void *
copy_f(void *x, void *args_)
{
    decrypt_args_t *args = args_;
    mife_ek_t *ek = args->ek;
    encoding *out;

    out = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
    encoding_set(ek->enc_vt, out, x);
    return out;
}

static void *
input_f(size_t i, void *args_)
{
    decrypt_args_t *args = args_;
    const size_t slot = circ_params_slot(args->cp, i);
    const size_t bit = circ_params_bit(args->cp, i);
    /* XXX: check that slot and bit are valid! */
    return copy_f(args->cts[slot]->xhat[bit], args_);
}

static void *
const_f(size_t i, long val, void *args_)
{
    (void) val;
    decrypt_args_t *args = args_;
    const size_t bit = circ_params_bit(args->cp, acirc_ninputs(args->cp->circ) + i);
    /* XXX: check that bit is valid! */
    return copy_f(args->ek->constants->xhat[bit], args_);
}

static void *
eval_f(acirc_op op, const void *x_, const void *y_, void *args_)
{
    decrypt_args_t *args = args_;
    mife_ek_t *ek = args->ek;
    const encoding *x = x_;
    const encoding *y = y_;
    encoding *res;

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

static void
free_f(void *x, void *args_)
{
    decrypt_args_t *args = args_;
    if (x)
        encoding_free(args->ek->enc_vt, x);
}

/* static void */
/* decrypt_worker(void *vargs) */
/* { */
/*     decrypt_args_t *const dargs = vargs; */
/*     const acircref ref = dargs->ref; */
/*     const acirc *const c = dargs->c; */
/*     mife_ciphertext_t **cts = dargs->cts; */
/*     const mife_ek_t *const ek = dargs->ek; */
/*     bool *const mine = dargs->mine; */
/*     int *const ready = dargs->ready; */
/*     encoding **cache = dargs->cache; */
/*     ref_list *const deps = dargs->deps; */
/*     threadpool *const pool = dargs->pool; */
/*     int *const rop = dargs->rop; */
/*     size_t *const kappas = dargs->kappas; */

/*     const circ_params_t *const cp = ek->cp; */
/*     const acirc_operation op = c->gates.gates[ref].op; */
/*     const acircref *const args = c->gates.gates[ref].args; */
/*     encoding *res = NULL; */

/*     switch (op) { */
/*     case OP_CONST: { */
/*         const size_t bit = circ_params_bit(cp, cp->circ->ninputs + args[0]); */
/*         /\* XXX: check that bit is valid! *\/ */
/*         res = ek->constants->xhat[bit]; */
/*         mine[ref] = false; */
/*         break; */
/*     } */
/*     case OP_INPUT: { */
/*         const size_t slot = circ_params_slot(cp, args[0]); */
/*         const size_t bit = circ_params_bit(cp, args[0]); */
/*         /\* XXX: check that slot and bit are valid! *\/ */
/*         res = cts[slot]->xhat[bit]; */
/*         mine[ref] = false; */
/*         break; */
/*     } */
/*     case OP_ADD: case OP_SUB: case OP_MUL: { */
/*         res = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp); */
/*         mine[ref] = true; */

/*         const encoding *const x = cache[args[0]]; */
/*         const encoding *const y = cache[args[1]]; */

/*         if (op == OP_MUL) { */
/*             encoding_mul(ek->enc_vt, ek->pp_vt, res, x, y, ek->pp); */
/*         } else { */
/*             encoding *tmp_x, *tmp_y; */
/*             tmp_x = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp); */
/*             tmp_y = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp); */
/*             encoding_set(ek->enc_vt, tmp_x, x); */
/*             encoding_set(ek->enc_vt, tmp_y, y); */
/*             if (!index_set_eq(ek->enc_vt->mmap_set(tmp_x), ek->enc_vt->mmap_set(tmp_y))) */
/*                 raise_encodings(ek, tmp_x, tmp_y); */
/*             if (op == OP_ADD) { */
/*                 encoding_add(ek->enc_vt, ek->pp_vt, res, tmp_x, tmp_y, ek->pp); */
/*             } else { */
/*                 encoding_sub(ek->enc_vt, ek->pp_vt, res, tmp_x, tmp_y, ek->pp); */
/*             } */
/*             encoding_free(ek->enc_vt, tmp_x); */
/*             encoding_free(ek->enc_vt, tmp_y); */
/*         } */
/*         break; */
/*     } */
/*     default: */
/*         fprintf(stderr, "fatal: op not supported\n"); */
/*         abort(); */
/*     } */

/*     cache[ref] = res; */

/*     ref_list_node *node = &deps->refs[ref]; */
/*     for (size_t i = 0; i < node->cur; ++i) { */
/*         const int num = __sync_add_and_fetch(&ready[node->refs[i]], 1); */
/*         if (num == 2) { */
/*             decrypt_args_t *newargs = my_calloc(1, sizeof newargs[0]); */
/*             memcpy(newargs, dargs, sizeof newargs[0]); */
/*             newargs->ref = node->refs[i]; */
/*             threadpool_add_job(pool, decrypt_worker, newargs); */
/*         } */
/*     } */

/*     free(dargs); */

/*     ssize_t output = -1; */
/*     for (size_t i = 0; i < cp->m; i++) { */
/*         if (ref == c->outputs.buf[i]) { */
/*             output = i; */
/*             break; */
/*         } */
/*     } */

/*     if (output != -1) { */
/*         encoding *out, *lhs, *rhs; */
/*         const index_set *const toplevel = ek->pp_vt->toplevel(ek->pp); */
/*         int result; */

/*         out = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp); */
/*         lhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp); */
/*         rhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp); */

/*         /\* Compute LHS *\/ */
/*         encoding_mul(ek->enc_vt, ek->pp_vt, lhs, res, ek->zhat[output], ek->pp); */
/*         raise_encoding(ek, lhs, toplevel); */
/*         if (!index_set_eq(ek->enc_vt->mmap_set(lhs), toplevel)) { */
/*             fprintf(stderr, "error: lhs != toplevel\n"); */
/*             index_set_print(ek->enc_vt->mmap_set(lhs)); */
/*             index_set_print(toplevel); */
/*             if (rop) */
/*                 rop[output] = 1; */
/*             goto cleanup; */
/*         } */
/*         /\* Compute RHS *\/ */
/*         if (ek->Chatstar) { */
/*             encoding_set(ek->enc_vt, rhs, ek->Chatstar); */
/*             for (size_t i = 0; i < cp->n; ++i) */
/*                 encoding_mul(ek->enc_vt, ek->pp_vt, rhs, rhs, cts[i]->what[output], ek->pp); */
/*         } else { */
/*             encoding_set(ek->enc_vt, rhs, cts[0]->what[output]); */
/*             for (size_t i = 1; i < cp->n - 1; ++i) */
/*                 encoding_mul(ek->enc_vt, ek->pp_vt, rhs, rhs, cts[i]->what[output], ek->pp); */
/*         } */
/*         if (!index_set_eq(ek->enc_vt->mmap_set(rhs), toplevel)) { */
/*             fprintf(stderr, "error: rhs != toplevel\n"); */
/*             index_set_print(ek->enc_vt->mmap_set(rhs)); */
/*             index_set_print(toplevel); */
/*             if (rop) */
/*                 rop[output] = 1; */
/*             goto cleanup; */
/*         } */
/*         encoding_sub(ek->enc_vt, ek->pp_vt, out, lhs, rhs, ek->pp); */
/*         result = !encoding_is_zero(ek->enc_vt, ek->pp_vt, out, ek->pp); */
/*         if (rop) */
/*             rop[output] = result; */
/*         if (kappas) */
/*             kappas[output] = encoding_get_degree(ek->enc_vt, out); */

/*     cleanup: */
/*         encoding_free(ek->enc_vt, out); */
/*         encoding_free(ek->enc_vt, lhs); */
/*         encoding_free(ek->enc_vt, rhs); */
/*     } */
/* } */

int
mife_decrypt(const mife_ek_t *ek, long *rop, mife_ciphertext_t **cts,
             size_t nthreads, size_t *kappa)
{
    encoding **encs = NULL;
    const circ_params_t *cp = ek->cp;
    acirc_t *circ = cp->circ;
    int ret = OK;

    if (ek == NULL || cts == NULL)
        return ERR;

    size_t *kappas = NULL;

    if (kappa)
        kappas = my_calloc(cp->m, sizeof kappas[0]);

    {
        decrypt_args_t args = {
            .cp = cp,
            .cts = cts,
            .ek = ek,
        };
        encs = (encoding **) acirc_traverse(circ, input_f, const_f, eval_f, copy_f, free_f, &args, nthreads);
    }

    for (size_t o = 0; o < acirc_noutputs(circ); ++o) {
        encoding *out, *lhs, *rhs;
        const index_set *const toplevel = ek->pp_vt->toplevel(ek->pp);
        int result;

        out = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        lhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        rhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);

        /* Compute LHS */
        encoding_mul(ek->enc_vt, ek->pp_vt, lhs, encs[o], ek->zhat[o], ek->pp);
        raise_encoding(ek, lhs, toplevel);
        if (!index_set_eq(ek->enc_vt->mmap_set(lhs), toplevel)) {
            fprintf(stderr, "error: lhs != toplevel\n");
            index_set_print(ek->enc_vt->mmap_set(lhs));
            index_set_print(toplevel);
            if (rop)
                rop[o] = 1;
            ret = ERR;
            goto cleanup;
        }
        /* Compute RHS */
        if (ek->Chatstar) {
            encoding_set(ek->enc_vt, rhs, ek->Chatstar);
            for (size_t i = 0; i < cp->n; ++i)
                encoding_mul(ek->enc_vt, ek->pp_vt, rhs, rhs, cts[i]->what[o], ek->pp);
        } else {
            encoding_set(ek->enc_vt, rhs, cts[0]->what[o]);
            for (size_t i = 1; i < cp->n - 1; ++i)
                encoding_mul(ek->enc_vt, ek->pp_vt, rhs, rhs, cts[i]->what[o], ek->pp);
        }
        if (!index_set_eq(ek->enc_vt->mmap_set(rhs), toplevel)) {
            fprintf(stderr, "error: rhs != toplevel\n");
            index_set_print(ek->enc_vt->mmap_set(rhs));
            index_set_print(toplevel);
            if (rop)
                rop[o] = 1;
            ret = ERR;
            goto cleanup;
        }
        encoding_sub(ek->enc_vt, ek->pp_vt, out, lhs, rhs, ek->pp);
        result = !encoding_is_zero(ek->enc_vt, ek->pp_vt, out, ek->pp);
        if (rop)
            rop[o] = result;
        if (kappas)
            kappas[o] = encoding_get_degree(ek->enc_vt, out);

    cleanup:
        encoding_free(ek->enc_vt, out);
        encoding_free(ek->enc_vt, lhs);
        encoding_free(ek->enc_vt, rhs);
        if (ret == ERR)
            break;
    }

    if (encs) {
        for (size_t o = 0; o < acirc_noutputs(circ); ++o) {
            encoding_free(ek->enc_vt, encs[o]);
        }
        free(encs);
    }

    if (kappa) {
        size_t maxkappa = 0;
        for (size_t i = 0; i < cp->m; i++) {
            if (kappas[i] > maxkappa)
                maxkappa = kappas[i];
        }
        free(kappas);
        *kappa = maxkappa;
    }

    return ret;
}
