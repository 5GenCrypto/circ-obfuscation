#include "mife.h"

#include "index_set.h"
#include "mife_params.h"
#include "reflist.h"
#include "vtables.h"
#include "util.h"

#include <assert.h>
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
    size_t npowers;
    encoding **xhat;             /* [d_i] */
    encoding **uhat;             /* [npowers] */
    encoding **what;             /* [m] */
} mife_ciphertext_t;

typedef struct encode_args_t {
    const encoding_vtable *vt;
    encoding *enc;
    mpz_t *inps;
    size_t nslots;
    index_set *ix;
    const secret_params *sp;
    pthread_mutex_t *count_lock;
    size_t *count;
    size_t total;
} encode_args_t;

typedef struct decrypt_args_t {
    const mmap_vtable *mmap;
    acircref ref;
    const acirc *c;
    const mife_ciphertext_t **cts;
    const mife_ek_t *ek;
    bool *mine;
    int *ready;
    void *cache;
    ref_list **deps;
    threadpool *pool;
    int *rop;
    size_t *kappas;
} decrypt_args_t;


static void encode_worker(void *wargs)
{
    encode_args_t *const args = wargs;

    encode(args->vt, args->enc, args->inps, args->nslots, args->ix, args->sp);
    if (g_verbose) {
        pthread_mutex_lock(args->count_lock);
        print_progress(++*args->count, args->total);
        /* encoding_print(args->vt, args->enc); */
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
    encode_args_t *args = my_calloc(1, sizeof args[0]);
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
           size_t secparam, aes_randstate_t rng, size_t *kappa, size_t nthreads)
{
    mife_t *mife;

    acirc *const circ = cp->circ;

    mife = my_calloc(1, sizeof mife[0]);
    mife->mmap = mmap;
    mife->cp = cp;
    mife->enc_vt = get_encoding_vtable(mmap);
    mife->pp_vt = get_pp_vtable(mmap);
    mife->sp_vt = get_sp_vtable(mmap);
    mife->sp = secret_params_new(mife->sp_vt, cp, secparam, kappa, nthreads, rng);
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
    threadpool *pool = threadpool_create(nthreads);
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
            if (i >= circ->ninputs) {
                /* these are constants */
                vdeg[i][o] = acirc_max_const_degree(circ);
            } else {
                vdeg[i][o] = acirc_var_degree(circ, circ->outputs.buf[o], i, memo);
            }
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
            IX_X(ix, cp, i) = vdeg_max[i];
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
            IX_W(ix, cp, i) = 1;
            IX_X(ix, cp, i) = vdeg_max[i] - vdeg[i][o];
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
    double start, end;
    mife_sk_t *sk;

    sk = my_calloc(1, sizeof sk[0]);
    sk->mmap = mmap;
    sk->cp = cp;
    sk->enc_vt = get_encoding_vtable(mmap);
    sk->pp_vt = get_pp_vtable(mmap);
    sk->sp_vt = get_sp_vtable(mmap);
    start = current_time();
    sk->pp = public_params_fread(sk->pp_vt, cp, fp);
    end = current_time();
    if (g_verbose)
        fprintf(stderr, "  Reading pp from disk: %.2fs\n", end - start);
    start = current_time();
    sk->sp = secret_params_fread(sk->sp_vt, cp, fp);
    end = current_time();
    if (g_verbose)
        fprintf(stderr, "  Reading sp from disk: %.2fs\n", end - start);

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
    }
    for (size_t p = 0; p < ct->npowers; ++p) {
        encoding_free(ct->enc_vt, ct->uhat[p]);
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
    fprintf(fp, "%lu\n", ct->npowers);
    for (size_t j = 0; j < ninputs; ++j) {
        encoding_fwrite(ct->enc_vt, ct->xhat[j], fp);
    }
    for (size_t p = 0; p < ct->npowers; ++p) {
        encoding_fwrite(ct->enc_vt, ct->uhat[p], fp);
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
    if (fscanf(fp, "%lu\n", &ct->slot) != 1) {
        fprintf(stderr, "error (%s): cannot read slot number\n", __func__);
        goto error;
    }
    if (ct->slot >= cp->n) {
        fprintf(stderr, "error (%s): slot number > number of slots\n", __func__);
        goto error;
    }
    if (fscanf(fp, "%lu\n", &ct->npowers) != 1) {
        fprintf(stderr, "error (%s): cannot read number of powers\n", __func__);
        goto error;
    }
    ninputs = cp->ds[ct->slot];
    ct->xhat = my_calloc(ninputs, sizeof ct->xhat[0]);
    for (size_t j = 0; j < ninputs; ++j) {
        ct->xhat[j] = encoding_fread(ct->enc_vt, fp);
    }
    ct->uhat = my_calloc(ct->npowers, sizeof ct->uhat[0]);
    for (size_t p = 0; p < ct->npowers; ++p) {
        ct->uhat[p] = encoding_fread(ct->enc_vt, fp);
    }
    ct->what = my_calloc(cp->m, sizeof ct->what[0]);
    for (size_t o = 0; o < cp->m; ++o) {
        ct->what[o] = encoding_fread(ct->enc_vt, fp);
    }

    return ct;
error:
    free(ct);
    return NULL;
}

mife_ciphertext_t *
mife_encrypt(const mife_sk_t *sk, size_t slot, const int *inputs,
             size_t npowers, size_t nthreads, aes_randstate_t rng)
{
    if (npowers == 0)
        return NULL;

    mife_ciphertext_t *ct;
    double start, end, _start, _end;

    if (g_verbose)
        fprintf(stderr, "  Encrypting...\n");

    start = current_time();
    _start = current_time();

    const circ_params_t *cp = sk->cp;
    const size_t ninputs = cp->ds[slot];
    mpz_t *const moduli =
        mpz_vect_create_of_fmpz(sk->mmap->sk->plaintext_fields(sk->sp->sk),
                                sk->mmap->sk->nslots(sk->sp->sk));

    ct = my_calloc(1, sizeof ct[0]);
    ct->enc_vt = sk->enc_vt;
    ct->slot = slot;
    ct->npowers = npowers;
    ct->xhat = my_calloc(ninputs, sizeof ct->xhat[0]);
    for (size_t j = 0; j < ninputs; ++j) {
        ct->xhat[j] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
    }
    ct->uhat = my_calloc(npowers, sizeof ct->uhat[0]);
    for (size_t p = 0; p < npowers; ++p) {
        ct->uhat[p] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
    }
    ct->what = my_calloc(cp->m, sizeof ct->what[0]);
    for (size_t o = 0; o < cp->m; ++o) {
        ct->what[o] = encoding_new(sk->enc_vt, sk->pp_vt, sk->pp);
    }

    mpz_t slots[1 + cp->n];
    mpz_t betas[ninputs];
    mpz_t circ_inputs[circ_params_ninputs(cp)];
    mpz_t consts[cp->circ->consts.n];
    mpz_t cs[cp->m];
    index_set *const ix = index_set_new(mife_params_nzs(cp));

    assert(slot <= cp->circ->ninputs);

    mpz_vect_init(slots, 1 + cp->n);
    for (size_t j = 0; j < ninputs; ++j) {
        mpz_init(betas[j]);
        mpz_randomm_inv(betas[j], rng, moduli[1 + slot]);
    }
    {
        size_t idx = 0;
        for (size_t i = 0; i < cp->n; ++i) {
            for (size_t j = 0; j < cp->ds[i]; ++j) {
                if (i == slot)
                    mpz_init_set(circ_inputs[idx + j], betas[j]);
                else
                    mpz_init_set_ui(circ_inputs[idx + j], 1);
            }
            idx += cp->ds[i];
        }
    }
    for (size_t i = 0; i < cp->circ->consts.n; ++i) {
        if (slot == cp->circ->ninputs)
            /* these are constants */
            mpz_init_set(consts[i], betas[i]);
        else
            mpz_init_set_ui(consts[i], 1);
    }
    {
        bool known[acirc_nrefs(cp->circ)];
        mpz_t cache[acirc_nrefs(cp->circ)];
        memset(known, '\0', sizeof known);
        mpz_vect_init(cache, acirc_nrefs(cp->circ));
        for (size_t o = 0; o < cp->m; ++o) {
            mpz_init(cs[o]);
            acirc_eval_mpz_mod_memo(cs[o], cp->circ, cp->circ->outputs.buf[o],
                                    circ_inputs, consts, moduli[1 + slot],
                                    known, cache);
        }
        mpz_vect_clear(cache, acirc_nrefs(cp->circ));
    }

    threadpool *pool = threadpool_create(nthreads);
    pthread_mutex_t count_lock;
    size_t count = 0;
    const size_t total = mife_params_num_encodings_encrypt(cp, slot, npowers);
    pthread_mutex_init(&count_lock, NULL);

    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Initialize: %.2fs\n", _end - _start);

    _start = current_time();
    
    /* Encode \hat xⱼ */
    index_set_clear(ix);
    IX_X(ix, cp, slot) = 1;
    for (size_t j = 0; j < ninputs; ++j) {
        if (slot == cp->circ->ninputs) {
            /* these are constants */
            mpz_set_ui(slots[0], cp->circ->consts.buf[j]);
        } else {
            mpz_set_ui(slots[0], inputs[j]);
        }
        for (size_t i = 0; i < cp->n; ++i) {
            mpz_set_ui(slots[1 + i], 1);
        }
        mpz_set(slots[1 + slot], betas[j]);
        /* Encode \hat xⱼ := [xⱼ, 1, ..., 1, βⱼ, 1, ..., 1] */
        __encode(pool, sk->enc_vt, ct->xhat[j], slots, 1 + cp->n,
                 index_set_copy(ix), sk->sp, &count_lock, &count, total);
    }
    /* Encode \hat u_p */
    index_set_clear(ix);
    mpz_set_ui(slots[0], 1);
    mpz_set_ui(slots[1 + slot], 1);
    for (size_t p = 0; p < ct->npowers; ++p) {
        IX_X(ix, cp, slot) = 1 << p;
        /* Encode \hat u_p = [1, ..., 1] */
        __encode(pool, sk->enc_vt, ct->uhat[p], slots, 1 + cp->n,
                 index_set_copy(ix), sk->sp, &count_lock, &count, total);
    }
    /* Encode \hat wₒ */
    index_set_clear(ix);
    IX_W(ix, cp, slot) = 1;
    mpz_set_ui(slots[0], 0);
    for (size_t o = 0; o < cp->m; ++o) {
        mpz_set(slots[1 + slot], cs[o]);
        /* Encode \hat wₒ = [0, 1, ..., 1, C†ₒ, 1, ..., 1] */
        __encode(pool, sk->enc_vt, ct->what[o], slots, 1 + cp->n,
                 index_set_copy(ix), sk->sp, &count_lock, &count, total);
    }

    threadpool_destroy(pool);
    pthread_mutex_destroy(&count_lock);

    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Encode: %.2fs\n", _end - _start);

    index_set_free(ix);
    mpz_vect_clear(slots, 1 + cp->n);
    mpz_vect_clear(betas, ninputs);
    mpz_vect_clear(circ_inputs, circ_params_ninputs(cp));
    mpz_vect_clear(consts, cp->circ->consts.n);
    mpz_vect_clear(cs, cp->m);
    mpz_vect_free(moduli, sk->mmap->sk->nslots(sk->sp->sk));

    end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Total: %.2fs\n", end - start);
    
    return ct;
}

static void
_raise_encoding(const mife_ek_t *ek, encoding *x, encoding **us, size_t npowers,
                size_t diff)
{
    assert(npowers > 0);
    while (diff > 0) {
        size_t p = 0;
        while (((size_t) 1 << (p+1)) <= diff && (p+1) < npowers)
            p++;
        encoding_mul(ek->enc_vt, ek->pp_vt, x, x, us[p], ek->pp);
        diff -= (1 << p);
    }
}

static void
raise_encoding(const mife_ek_t *ek, const mife_ciphertext_t **cts,
               encoding *x, const index_set *target)
{
    const circ_params_t *cp = ek->cp;

    index_set *const ix = index_set_difference(target, ek->enc_vt->mmap_set(x));
    size_t diff;
    for (size_t i = 0; i < cp->n; i++) {
        diff = IX_X(ix, cp, i);
        if (diff > 0) {
            _raise_encoding(ek, x, cts[i]->uhat, cts[i]->npowers, diff);
        }
    }
    index_set_free(ix);
}

static void
raise_encodings(const mife_ek_t *ek, const mife_ciphertext_t **cts,
                encoding *x, encoding *y)
{
    index_set *const ix = index_set_union(ek->enc_vt->mmap_set(x),
                                          ek->enc_vt->mmap_set(y));
    raise_encoding(ek, cts, x, ix);
    raise_encoding(ek, cts, y, ix);
    index_set_free(ix);
}

static void
decrypt_worker(void *vargs)
{
    decrypt_args_t *dargs = vargs;
    const acircref ref = dargs->ref;
    const acirc *const c = dargs->c;
    const mife_ciphertext_t **cts = dargs->cts;
    const mife_ek_t *ek = dargs->ek;
    bool *const mine = dargs->mine;
    int *const ready = dargs->ready;
    encoding **cache = dargs->cache;
    ref_list *const *const deps = dargs->deps;
    threadpool *const pool = dargs->pool;
    int *const rop = dargs->rop;
    size_t *const kappas = dargs->kappas;

    const acirc_operation op = c->gates.gates[ref].op;
    const acircref *const args = c->gates.gates[ref].args;
    encoding *res;

    const circ_params_t *cp = ek->cp;
    const size_t ninputs = cp->n;
    const size_t noutputs = cp->m;

    switch (op) {
    case OP_INPUT: {
        const size_t slot = circ_params_slot(cp, args[0]);
        const size_t bit = circ_params_bit(cp, args[0]);
        res = cts[slot]->xhat[bit];
        mine[ref] = false;
        break;
    }
    case OP_CONST: {
        const size_t idx = cp->circ->ninputs;
        const size_t slot = circ_params_slot(cp, args[0] + idx);
        const size_t bit = circ_params_bit(cp, args[0] + idx);
        res = cts[slot]->xhat[bit];
        mine[ref] = false;
        break;
    }
    case OP_ADD: case OP_SUB: case OP_MUL: {
        assert(c->gates.gates[ref].nargs == 2);
        res = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        mine[ref] = true;

        // the encodings of the args exist since the ref's children signalled it
        const encoding *const x = cache[args[0]];
        const encoding *const y = cache[args[1]];

        if (op == OP_MUL) {
            encoding_mul(ek->enc_vt, ek->pp_vt, res, x, y, ek->pp);
        } else {
            encoding *tmp_x, *tmp_y;
            tmp_x = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
            tmp_y = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
            encoding_set(ek->enc_vt, tmp_x, x);
            encoding_set(ek->enc_vt, tmp_y, y);
            if (!index_set_eq(ek->enc_vt->mmap_set(tmp_x), ek->enc_vt->mmap_set(tmp_y)))
                raise_encodings(ek, cts, tmp_x, tmp_y);
            if (op == OP_ADD) {
                encoding_add(ek->enc_vt, ek->pp_vt, res, tmp_x, tmp_y, ek->pp);
            } else if (op == OP_SUB) {
                encoding_sub(ek->enc_vt, ek->pp_vt, res, tmp_x, tmp_y, ek->pp);
            } else {
                abort();
            }
            encoding_free(ek->enc_vt, tmp_x);
            encoding_free(ek->enc_vt, tmp_y);
        }
        break;
    }
    default:
        fprintf(stderr, "fatal: op not supported\n");
        abort();
    }

    cache[ref] = res;

    ref_list_node *cur = deps[ref]->first;
    while (cur != NULL) {
        const int num = __sync_add_and_fetch(&ready[cur->ref], 1);
        if (num == 2) {
            decrypt_args_t *newargs = my_calloc(1, sizeof newargs[0]);
            memcpy(newargs, dargs, sizeof newargs[0]);
            newargs->ref = cur->ref;
            threadpool_add_job(pool, decrypt_worker, newargs);
        } else {
            cur = cur->next;
        }
    }

    free(dargs);

    ssize_t output = -1;
    for (size_t i = 0; i < noutputs; i++) {
        if (ref == c->outputs.buf[i]) {
            output = i;
            break;
        }
    }

    if (output != -1) {
        encoding *out, *lhs, *rhs, *tmp;
        const index_set *const toplevel = ek->pp_vt->toplevel(ek->pp);
        int result;

        out = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        lhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        rhs = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);
        tmp = encoding_new(ek->enc_vt, ek->pp_vt, ek->pp);

        /* Compute RHS */
        encoding_set(ek->enc_vt, rhs, ek->Chatstar);
        for (size_t i = 0; i < ninputs; ++i) {
            encoding_set(ek->enc_vt, tmp, rhs);
            encoding_mul(ek->enc_vt, ek->pp_vt, rhs, tmp, cts[i]->what[output], ek->pp);
        }
        if (!index_set_eq(ek->enc_vt->mmap_set(rhs), toplevel)) {
            fprintf(stderr, "error: rhs != toplevel\n");
            index_set_print(ek->enc_vt->mmap_set(rhs));
            index_set_print(toplevel);
            if (rop)
                rop[output] = 1;
            goto cleanup;
        }
        /* Compute LHS */
        encoding_mul(ek->enc_vt, ek->pp_vt, lhs, res, ek->zhat[output], ek->pp);
        raise_encoding(ek, cts, lhs, toplevel);
        if (!index_set_eq(ek->enc_vt->mmap_set(lhs), toplevel)) {
            fprintf(stderr, "error: lhs != toplevel\n");
            index_set_print(ek->enc_vt->mmap_set(lhs));
            index_set_print(toplevel);
            if (rop)
                rop[output] = 1;
            goto cleanup;
        }
        encoding_sub(ek->enc_vt, ek->pp_vt, out, lhs, rhs, ek->pp);
        result = !encoding_is_zero(ek->enc_vt, ek->pp_vt, out, ek->pp);
        if (rop)
            rop[output] = result;
        kappas[output] = encoding_get_degree(ek->enc_vt, out);

    cleanup:
        encoding_free(ek->enc_vt, out);
        encoding_free(ek->enc_vt, lhs);
        encoding_free(ek->enc_vt, rhs);
        encoding_free(ek->enc_vt, tmp);
    }
}

int
mife_decrypt(const mife_ek_t *ek, int *rop, const mife_ciphertext_t **cts,
             size_t nthreads, size_t *kappa)
{
    const circ_params_t *cp = ek->cp;
    const acirc *const c = cp->circ;
    int ret = ERR;

    encoding **cache = my_calloc(acirc_nrefs(c), sizeof cache[0]);
    bool *mine = my_calloc(acirc_nrefs(c), sizeof mine[0]);
    int *ready = my_calloc(acirc_nrefs(c), sizeof ready[0]);
    size_t *kappas = my_calloc(c->outputs.n, sizeof kappas[0]);
    ref_list **deps = ref_lists_new(c);
    threadpool *pool = threadpool_create(nthreads);

    for (size_t ref = 0; ref < acirc_nrefs(c); ++ref) {
        acirc_operation op = c->gates.gates[ref].op;
        if (!(op == OP_INPUT || op == OP_CONST))
            continue;
        decrypt_args_t *args = calloc(1, sizeof args[0]);
        args->mmap   = ek->mmap;
        args->ref    = ref;
        args->c      = c;
        args->cts    = cts;
        args->ek     = ek;
        args->mine   = mine;
        args->ready  = ready;
        args->cache  = cache;
        args->deps   = deps;
        args->pool   = pool;
        args->rop    = rop;
        args->kappas = kappas;
        threadpool_add_job(pool, decrypt_worker, args);
    }
    ret = OK;

    threadpool_destroy(pool);

    size_t maxkappa = 0;
    for (size_t i = 0; i < c->outputs.n; i++) {
        if (kappas[i] > maxkappa)
            maxkappa = kappas[i];
    }
    if (kappa)
        *kappa = maxkappa;

    for (size_t i = 0; i < acirc_nrefs(c); i++) {
        if (mine[i]) {
            encoding_free(ek->enc_vt, cache[i]);
        }
    }
    ref_lists_free(deps, c);
    free(cache);
    free(mine);
    free(ready);
    free(kappas);

    return ret;
}
