#include "mife.h"

#include "index_set.h"
#include "mife_params.h"
#include "vtables.h"
#include "util.h"

#include <string.h>
#include <threadpool.h>

typedef struct mife_t {
    const mmap_vtable *mmap;
    const circ_params_t *cp;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    secret_params *sp;
    public_params *pp;
    encoding *Chatstar;
    encoding **zhat;            /* [m] */
} mife_t;

typedef struct mife_sk_t {
    const mmap_vtable *mmap;
    const circ_params_t *cp;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    secret_params *sp;
    public_params *pp;
} mife_sk_t;

typedef struct mife_ek_t {
    const mmap_vtable *mmap;
    const circ_params_t *cp;
    const encoding_vtable *enc_vt;
    const pp_vtable *pp_vt;
    public_params *pp;
    encoding *Chatstar;
    encoding **zhat;            /* [m] */
} mife_ek_t;

typedef struct mife_ciphertext_t {
    const encoding_vtable *enc_vt;
    size_t slot;
    encoding **xhat;             /* [d_i] */
    encoding **uhat;             /* [d_i] */
    encoding **what;             /* [m] */
} mife_ciphertext_t;

typedef struct encode_args {
    const encoding_vtable *vt;
    encoding *enc;
    mpz_t *inps;
    size_t nslots;
    index_set *ix;
    const secret_params *sp;
    pthread_mutex_t *count_lock;
    size_t *count;
    size_t total;
} encode_args;

static void encode_worker(void *wargs)
{
    encode_args *const args = wargs;

    encode(args->vt, args->enc, args->inps, args->nslots, args->ix, args->sp);
    if (g_verbose) {
        pthread_mutex_lock(args->count_lock);
        print_progress(++*args->count, args->total);
        pthread_mutex_unlock(args->count_lock);
    }
    mpz_vect_free(args->inps, args->nslots);
    index_set_free(args->ix);
    free(args);
}

static void
__encode(threadpool *pool, const encoding_vtable *vt, encoding *enc, mpz_t *inps,
         size_t nslots, index_set *ix, const secret_params *sp,
         pthread_mutex_t *count_lock, size_t *count, size_t total)
{
    encode_args *args = my_calloc(1, sizeof args[0]);
    args->vt = vt;
    args->enc = enc;
    args->inps = mpz_vect_new(nslots);
    for (size_t i = 0; i < nslots; ++i) {
        mpz_set(args->inps[i], inps[i]);
    }
    args->nslots = nslots;
    args->ix = ix;
    args->sp = sp;
    args->count_lock = count_lock;
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
    }
    if (mife->pp)
        public_params_free(mife->pp_vt, mife->pp);
    if (mife->sp)
        secret_params_free(mife->sp_vt, mife->sp);

    free(mife);
}

mife_t *
mife_setup(const mmap_vtable *mmap, const circ_params_t *cp,
           size_t secparam, aes_randstate_t rng, size_t *kappa, size_t ncores)
{
    mife_t *mife;

    acirc *const circ = cp->circ;

    mife = my_calloc(1, sizeof mife[0]);
    mife->mmap = mmap;
    mife->cp = cp;
    mife->enc_vt = get_encoding_vtable(mmap);
    mife->pp_vt = get_pp_vtable(mmap);
    mife->sp_vt = get_sp_vtable(mmap);
    mife->sp = secret_params_new(mife->sp_vt, cp, secparam, kappa, ncores, rng);
    if (mife->sp == NULL) {
        mife_free(mife);
        return NULL;
    }
    mife->pp = public_params_new(mife->pp_vt, mife->sp_vt, mife->sp);
    if (mife->pp == NULL) {
        mife_free(mife);
        return NULL;
    }
    mife->Chatstar = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
    mife->zhat = my_calloc(cp->m, sizeof mife->zhat[0]);
    for (size_t o = 0; o < cp->m; ++o) {
        mife->zhat[o] = encoding_new(mife->enc_vt, mife->pp_vt, mife->pp);
    }

    mpz_t inps[1 + cp->n];
    index_set *const ix = index_set_new(mife_params_nzs(cp));
    threadpool *pool = threadpool_create(ncores);
    pthread_mutex_t count_lock;
    size_t count = 0;
    const size_t total = mife_params_num_encodings_setup(cp);
    pthread_mutex_init(&count_lock, NULL);
    mpz_vect_init(inps, 1 + cp->n);

    size_t vdeg[cp->n][cp->m];
    size_t vdeg_max[cp->n];
    acirc_memo *memo;

    memset(vdeg, '\0', sizeof vdeg);
    memset(vdeg_max, '\0', sizeof vdeg_max);

    memo = acirc_memo_new(circ);
    for (size_t i = 0; i < cp->n; ++i) {
        for (size_t o = 0; o < cp->m; ++o) {
            vdeg[i][o] = acirc_var_degree(circ, circ->outputs.buf[o], i, memo);
            if (vdeg[i][o] > vdeg_max[i])
                vdeg_max[i] = vdeg[i][o];
        }
    }
    acirc_memo_free(memo, circ);

    if (g_verbose) {
        print_progress(count, total);
    }
    
    {
        index_set_clear(ix);
        mpz_set_ui(inps[0], 0);
        for (size_t i = 0; i < cp->n; ++i) {
            mpz_set_ui(inps[1 + i], 1);
            IX_X(ix, i) = vdeg_max[i];
        }
        IX_Z(ix) = 1;
        __encode(pool, mife->enc_vt, mife->Chatstar, inps, 1 + cp->n,
                 index_set_copy(ix), mife->sp, &count_lock, &count, total);
    }

    for (size_t o = 0; o < cp->m; ++o) {
        index_set_clear(ix);
        mpz_set_ui(inps[0], 1); /* XXX: */
        for (size_t i = 0; i < cp->n; ++i) {
            mpz_set_ui(inps[1 + i], 1);
            IX_W(ix, i) = 1;
            IX_X(ix, i) = vdeg_max[i] = vdeg[i][o];
        }
        IX_Z(ix) = 1;
        __encode(pool, mife->enc_vt, mife->zhat[o], inps, 1 + cp->n,
                 index_set_copy(ix), mife->sp, &count_lock, &count, total);
    }

    threadpool_destroy(pool);
    pthread_mutex_destroy(&count_lock);

    index_set_free(ix);
    mpz_vect_clear(inps, 1 + cp->n);

    return mife;
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

    return sk;
}

void
mife_sk_free(mife_sk_t *sk)
{
    if (sk)
        free(sk);
}

int
mife_sk_fwrite(const mife_sk_t *sk, FILE *fp)
{
    public_params_fwrite(sk->pp_vt, sk->pp, fp);
    secret_params_fwrite(sk->sp_vt, sk->sp, fp);
    return OK;
}

mife_sk_t *
mife_sk_fread(const mmap_vtable *mmap, const circ_params_t *cp, FILE *fp)
{
    mife_sk_t *sk;

    sk = my_calloc(1, sizeof sk[0]);
    sk->mmap = mmap;
    sk->cp = cp;
    sk->enc_vt = get_encoding_vtable(mmap);
    sk->pp_vt = get_pp_vtable(mmap);
    sk->sp_vt = get_sp_vtable(mmap);
    sk->pp = public_params_fread(sk->pp_vt, cp, fp);
    sk->sp = secret_params_fread(sk->sp_vt, cp, fp);

    return sk;
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
    return ek;
}

void
mife_ek_free(mife_ek_t *ek)
{
    free(ek);
}

int
mife_ek_fwrite(const mife_ek_t *ek, FILE *fp)
{
    public_params_fwrite(ek->pp_vt, ek->pp, fp);
    encoding_fwrite(ek->enc_vt, ek->Chatstar, fp);
    for (size_t o = 0; o < ek->cp->m; ++o)
        encoding_fwrite(ek->enc_vt, ek->zhat[o], fp);
    return OK;
}

mife_ek_t *
mife_ek_fread(const mmap_vtable *mmap, const circ_params_t *cp, FILE *fp)
{
    mife_ek_t *ek;

    ek = my_calloc(1, sizeof ek[0]);
    ek->mmap = mmap;
    ek->cp = cp;
    ek->enc_vt = get_encoding_vtable(mmap);
    ek->pp_vt = get_pp_vtable(mmap);
    ek->pp = public_params_fread(ek->pp_vt, cp, fp);
    ek->Chatstar = encoding_fread(ek->enc_vt, fp);
    ek->zhat = my_calloc(cp->n, sizeof ek->zhat[0]);
    for (size_t o = 0; o < cp->m; ++o)
        ek->zhat[o] = encoding_fread(ek->enc_vt, fp);
    return ek;
}

void
mife_ciphertext_free(mife_ciphertext_t *ct, const circ_params_t *cp)
{
    if (ct == NULL)
        return;

    const size_t ninputs = cp->ds[ct->slot];

    for (size_t j = 0; j < ninputs; ++j) {
        encoding_free(ct->enc_vt, ct->xhat[j]);
        encoding_free(ct->enc_vt, ct->uhat[j]);
    }
    free(ct->xhat);
    free(ct->uhat);
    for (size_t o = 0; o < cp->m; ++o) {
        encoding_free(ct->enc_vt, ct->what[o]);
    }
    free(ct->what);
    free(ct);
}

int
mife_ciphertext_fwrite(const mife_ciphertext_t *ct, const circ_params_t *cp,
                       FILE *fp)
{
    const size_t ninputs = cp->ds[ct->slot];
    fprintf(fp, "%lu\n", ct->slot);
    for (size_t j = 0; j < ninputs; ++j) {
        encoding_fwrite(ct->enc_vt, ct->xhat[j], fp);
        encoding_fwrite(ct->enc_vt, ct->uhat[j], fp);
    }
    for (size_t o = 0; o < cp->m; ++o) {
        encoding_fwrite(ct->enc_vt, ct->what[o], fp);
    }

    return OK;
}

mife_ciphertext_t *
mife_ciphertext_fread(const mmap_vtable *mmap, const circ_params_t *cp, FILE *fp)
{
    mife_ciphertext_t *ct;
    size_t ninputs;

    ct = my_calloc(1, sizeof ct[0]);
    ct->enc_vt = get_encoding_vtable(mmap);
    fscanf(fp, "%lu\n", &ct->slot);
    ninputs = cp->ds[ct->slot];
    ct->xhat = my_calloc(ninputs, sizeof ct->xhat[0]);
    ct->uhat = my_calloc(ninputs, sizeof ct->uhat[0]);
    for (size_t j = 0; j < ninputs; ++j) {
        ct->xhat[j] = encoding_fread(ct->enc_vt, fp);
        ct->uhat[j] = encoding_fread(ct->enc_vt, fp);
    }
    ct->what = my_calloc(ninputs, sizeof ct->what[0]);
    for (size_t o = 0; o < cp->m; ++o) {
        ct->what[o] = encoding_fread(ct->enc_vt, fp);
    }

    return ct;
}

mife_ciphertext_t *
mife_encrypt(const mife_sk_t *sk, size_t slot, size_t *inputs, size_t ncores,
             aes_randstate_t rng)
{
    mife_ciphertext_t *ct;

    const circ_params_t *cp = sk->cp;
    const size_t ninputs = cp->ds[slot];
    mpz_t *const moduli =
        mpz_vect_create_of_fmpz(sk->mmap->sk->plaintext_fields(sk->sp->sk),
                                sk->mmap->sk->nslots(sk->sp->sk));

    ct = my_calloc(1, sizeof ct[0]);
    ct->enc_vt = sk->enc_vt;
    ct->slot = slot;
    ct->xhat = my_calloc(ninputs, sizeof ct->xhat[0]);
    ct->uhat = my_calloc(ninputs, sizeof ct->uhat[0]);
    for (size_t j = 0; j < ninputs; ++j) {
        ct->xhat[j] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
        ct->uhat[j] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
    }
    ct->what = my_calloc(ninputs, sizeof ct->what[0]);
    for (size_t o = 0; o < cp->m; ++o) {
        ct->what[o] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
    }

    mpz_t slots[1 + cp->n];
    mpz_t betas[ninputs];
    mpz_t circ_inputs[circ_params_ninputs(cp)];
    mpz_t cs[cp->m];
    index_set *const ix = index_set_new(mife_params_nzs(cp));

    mpz_vect_init(slots, 1 + cp->n);
    for (size_t j = 0; j < ninputs; ++j) {
        mpz_init(betas[j]);
        mpz_randomm_inv(betas[j], rng, moduli[1 + slot]);
    }
    mpz_vect_init(circ_inputs, circ_params_ninputs(cp));
    {
        size_t idx = 0;
        for (size_t i = 0; i < cp->n; ++i) {
            for (size_t j = 0; j < cp->ds[i]; ++j) {
                if (i == slot)
                    mpz_set(circ_inputs[idx + j], betas[j]);
                else
                    mpz_set_ui(circ_inputs[idx + j], 1);
            }
            idx += cp->ds[i];
        }
    }
    for (size_t o = 0; o < cp->m; ++o) {
        mpz_init(cs[o]);
        acirc_eval_mpz_mod(cs[o], cp->circ, cp->circ->outputs.buf[o],
                           circ_inputs, NULL, moduli[1 + slot]);
    }

    threadpool *pool = threadpool_create(ncores);
    pthread_mutex_t count_lock;
    size_t count = 0;
    const size_t total = mife_params_num_encodings_encrypt(cp, ninputs);
    pthread_mutex_init(&count_lock, NULL);

    index_set_clear(ix);
    IX_X(ix, slot) = 1;
    for (size_t j = 0; j < ninputs; ++j) {
        mpz_set_ui(slots[0], inputs[j]);
        for (size_t i = 0; i < cp->n; ++i) {
            mpz_set_ui(slots[1 + i], 1);
        }
        mpz_set(slots[1 + slot], betas[j]);
        __encode(pool, sk->enc_vt, ct->xhat[j], slots, 1 + cp->n,
                 index_set_copy(ix), sk->sp, &count_lock, &count, total);
        mpz_set_ui(slots[1 + slot], 1);
        __encode(pool, sk->enc_vt, ct->uhat[j], slots, 1 + cp->n,
                 index_set_copy(ix), sk->sp, &count_lock, &count, total);
    }
    index_set_clear(ix);
    IX_W(ix, slot) = 1;
    mpz_set_ui(slots[0], 0);
    for (size_t o = 0; o < cp->m; ++o) {
        mpz_set(slots[1 + slot], cs[o]);
        __encode(pool, sk->enc_vt, ct->what[o], slots, 1 + cp->n,
                 index_set_copy(ix), sk->sp, &count_lock, &count, total);
    }

    threadpool_destroy(pool);
    pthread_mutex_destroy(&count_lock);

    index_set_free(ix);
    mpz_vect_clear(slots, 1 + cp->n);
    mpz_vect_clear(betas, ninputs);
    mpz_vect_clear(circ_inputs, circ_params_ninputs(cp));
    mpz_vect_clear(cs, cp->m);
    mpz_vect_free(moduli, sk->mmap->sk->nslots(sk->sp->sk));
    
    return ct;
}



int
mife_decrypt(const mife_ek_t *ek, int *rop, mife_ciphertext_t **cts,
             size_t nthreads)
{
    return ERR;
}
