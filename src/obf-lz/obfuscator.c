#include "obfuscator.h"
#include "obf_params.h"
#include "../vtables.h"
#include "../util.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <threadpool.h>

struct obfuscation {
    const mmap_vtable *mmap;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    const encoding_vtable *enc_vt;
    const obf_params_t *op;
    secret_params *sp;
    public_params *pp;
    encoding ****shat;          // [c][Σ][ℓ]
    encoding ****uhat;          // [c][Σ][npowers]
    encoding ****zhat;          // [c][Σ][γ]
    encoding ****what;          // [c][Σ][γ]
    encoding **yhat;            // [m]
    encoding **vhat;            // [npowers]
    encoding **Chatstar;        // [γ]
};

typedef struct {
    const encoding_vtable *vt;
    encoding *enc;
    mpz_t inps[2];
    index_set *ix;
    const secret_params *sp;
    pthread_mutex_t *count_lock;
    size_t *count;
    size_t total;
} obf_args;

static void obf_worker(void *wargs)
{
    obf_args *const args = wargs;

    encode(args->vt, args->enc, args->inps, 2, args->ix, args->sp, 0);
    if (g_verbose) {
        pthread_mutex_lock(args->count_lock);
        print_progress(++*args->count, args->total);
        pthread_mutex_unlock(args->count_lock);
    }
    mpz_vect_clear(args->inps, 2);
    index_set_free(args->ix);
    free(args);
}

static void
__encode(threadpool *pool, const encoding_vtable *vt, encoding *enc, mpz_t inps[2],
         index_set *ix, const secret_params *sp, pthread_mutex_t *count_lock,
         size_t *count, size_t total)
{
    obf_args *args = my_calloc(1, sizeof args[0]);
    args->vt = vt;
    args->enc = enc;
    mpz_vect_init(args->inps, 2);
    mpz_set(args->inps[0], inps[0]);
    mpz_set(args->inps[1], inps[1]);
    args->ix = ix;
    args->sp = sp;
    args->count_lock = count_lock;
    args->count = count;
    args->total = total;
    threadpool_add_job(pool, obf_worker, args);
}

static obfuscation *
_alloc(const mmap_vtable *mmap, const obf_params_t *op)
{
    obfuscation *obf;

    const acirc_t *circ = op->circ;
    const size_t nsymbols = acirc_nsymbols(circ);
    const size_t nconsts = acirc_nconsts(circ);
    const size_t noutputs = acirc_noutputs(circ);

    obf = my_calloc(1, sizeof obf[0]);
    obf->mmap = mmap;
    obf->enc_vt = get_encoding_vtable(mmap);
    obf->pp_vt = get_pp_vtable(mmap);
    obf->sp_vt = get_sp_vtable(mmap);
    obf->op = op;
    obf->shat = my_calloc(nsymbols, sizeof obf->shat[0]);
    obf->uhat = my_calloc(nsymbols, sizeof obf->uhat[0]);
    obf->zhat = my_calloc(nsymbols, sizeof obf->zhat[0]);
    obf->what = my_calloc(nsymbols, sizeof obf->what[0]);
    for (size_t k = 0; k < nsymbols; k++) {
        obf->shat[k] = my_calloc(acirc_symnum(circ, k), sizeof obf->shat[0][0]);
        obf->uhat[k] = my_calloc(acirc_symnum(circ, k), sizeof obf->uhat[0][0]);
        obf->zhat[k] = my_calloc(acirc_symnum(circ, k), sizeof obf->zhat[0][0]);
        obf->what[k] = my_calloc(acirc_symnum(circ, k), sizeof obf->what[0][0]);
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            obf->shat[k][s] = my_calloc(acirc_symlen(circ, k), sizeof obf->shat[0][0][0]);
            obf->uhat[k][s] = my_calloc(op->npowers, sizeof obf->uhat[0][0][0]);
            obf->zhat[k][s] = my_calloc(noutputs, sizeof obf->zhat[0][0][0]);
            obf->what[k][s] = my_calloc(noutputs, sizeof obf->what[0][0][0]);
        }
    }
    obf->yhat = my_calloc(nconsts, sizeof obf->yhat[0]);
    obf->vhat = my_calloc(op->npowers, sizeof obf->vhat[0]);
    obf->Chatstar = my_calloc(noutputs, sizeof obf->Chatstar[0]);

    return obf;
}

static void
_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const obf_params_t *op = obf->op;
    const acirc_t *circ = op->circ;
    const size_t nsymbols = acirc_nsymbols(circ);
    const size_t nconsts = acirc_nconsts(circ);
    const size_t noutputs = acirc_noutputs(circ);

    for (size_t k = 0; k < nsymbols; k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            for (size_t j = 0; j < acirc_symlen(circ, k); j++)
                encoding_free(obf->enc_vt, obf->shat[k][s][j]);
            for (size_t p = 0; p < op->npowers; p++)
                encoding_free(obf->enc_vt, obf->uhat[k][s][p]);
            for (size_t o = 0; o < noutputs; o++) {
                encoding_free(obf->enc_vt, obf->zhat[k][s][o]);
                encoding_free(obf->enc_vt, obf->what[k][s][o]);
            }
            free(obf->uhat[k][s]);
            free(obf->shat[k][s]);
            free(obf->zhat[k][s]);
            free(obf->what[k][s]);
        }
        free(obf->shat[k]);
        free(obf->uhat[k]);
        free(obf->zhat[k]);
        free(obf->what[k]);
    }
    free(obf->shat);
    free(obf->uhat);
    free(obf->zhat);
    free(obf->what);
    for (size_t i = 0; i < nconsts; i++)
        encoding_free(obf->enc_vt, obf->yhat[i]);
    free(obf->yhat);
    for (size_t p = 0; p < op->npowers; p++)
        encoding_free(obf->enc_vt, obf->vhat[p]);
    free(obf->vhat);
    for (size_t i = 0; i < noutputs; i++)
        encoding_free(obf->enc_vt, obf->Chatstar[i]);
    free(obf->Chatstar);

    if (obf->pp)
        public_params_free(obf->pp_vt, obf->pp);
    if (obf->sp)
        secret_params_free(obf->sp_vt, obf->sp);

    free(obf);
}

static obfuscation *
_obfuscate(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    obfuscation *obf;

    const acirc_t *circ = op->circ;
    const size_t nsymbols = acirc_nsymbols(circ);
    const size_t nconsts = acirc_nconsts(circ);
    const size_t noutputs = acirc_noutputs(circ);
    index_set *ix;

    if (op->npowers == 0 || secparam == 0)
        return NULL;

    obf = _alloc(mmap, op);
    obf->sp = secret_params_new(obf->sp_vt, op, circ, secparam, kappa, nthreads, rng);
    if (obf->sp == NULL) {
        _free(obf);
        return NULL;
    }
    obf->pp = public_params_new(obf->pp_vt, obf->sp_vt, obf->sp, op);
    if (obf->pp == NULL) {
        _free(obf);
        return NULL;
    }

    for (size_t k = 0; k < nsymbols; k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            for (size_t j = 0; j < acirc_symlen(circ, k); j++) {
                obf->shat[k][s][j] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
            for (size_t p = 0; p < op->npowers; p++) {
                obf->uhat[k][s][p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
            for (size_t o = 0; o < noutputs; o++) {
                obf->zhat[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
                obf->what[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }
    for (size_t i = 0; i < nconsts; i++)
        obf->yhat[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t p = 0; p < op->npowers; p++)
        obf->vhat[p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < noutputs; i++)
        obf->Chatstar[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

    const mpz_t *moduli = (const mpz_t *) obf->mmap->sk->plaintext_fields(obf->sp->sk);

    size_t ell = 0, q = 0;
    for (size_t i = 0; i < nsymbols; ++i)
        ell = ell > acirc_symlen(circ, i) ? ell : acirc_symlen(circ, i);
    for (size_t i = 0; i < nsymbols; ++i)
        q = q > acirc_symnum(circ, i) ? q : acirc_symnum(circ, i);

    mpz_t inps[2];
    mpz_t **alpha;
    mpz_t **beta = NULL;
    mpz_t gamma[nsymbols][q][noutputs];
    mpz_t delta[nsymbols][q][noutputs];
    mpz_t Cstar[noutputs];
    threadpool *pool = threadpool_create(nthreads);

    alpha = calloc(nsymbols * ell, sizeof alpha[0]);
    for (size_t i = 0; i < nsymbols * ell; ++i)
        alpha[i] = mpz_vect_new(1);

    pthread_mutex_t count_lock;
    size_t count = 0;
    const size_t total = obf_params_num_encodings(op);

    pthread_mutex_init(&count_lock, NULL);
    mpz_vect_init(inps, 2);

    assert(obf->mmap->sk->nslots(obf->sp->sk) >= 2);

    for (size_t k = 0; k < nsymbols; k++) {
        for (size_t j = 0; j < acirc_symlen(circ, k); j++) {
            mpz_randomm_inv(*alpha[k * acirc_symlen(circ, k) + j], rng, moduli[1]);
        }
    }
    for (size_t k = 0; k < nsymbols; k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            for (size_t o = 0; o < noutputs; o++) {
                mpz_inits(gamma[k][s][o], delta[k][s][o], NULL);
                mpz_randomm_inv(delta[k][s][o], rng, moduli[0]);
                mpz_randomm_inv(gamma[k][s][o], rng, moduli[1]);
            }
        }
    }

    if (nconsts) {
        beta = calloc(nconsts, sizeof beta[0]);
        for (size_t i = 0; i < nconsts; i++) {
            beta[i] = mpz_vect_new(1);
            mpz_randomm_inv(*beta[i], rng, moduli[1]);
        }
    }

    size_t *const_deg;
    size_t const_deg_max = 0;
    size_t *var_deg[nsymbols];
    size_t var_deg_max[nsymbols];

    memset(var_deg, '\0', sizeof var_deg);
    memset(var_deg_max, '\0', sizeof var_deg_max);

    const_deg = acirc_const_degrees(circ);
    for (size_t o = 0; o < noutputs; o++) {
        if (const_deg[o] > const_deg_max)
            const_deg_max = const_deg[o];
    }

    for (size_t k = 0; k < nsymbols; ++k) {
        var_deg[k] = acirc_var_degrees(circ, k);
        for (size_t o = 0; o < noutputs; o++) {
            if (var_deg[k][o] > var_deg_max[k])
                var_deg_max[k] = var_deg[k][o];
        }
    }

    if (g_verbose)
        print_progress(count, total);

    for (size_t k = 0; k < nsymbols; k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            for (size_t j = 0; j < acirc_symlen(circ, k); j++) {
                ix = index_set_new(obf_params_nzs(circ));
                mpz_set_ui(inps[0], op->sigma ? s == j : bit(s, j));
                mpz_set   (inps[1], *alpha[k * acirc_symlen(circ, k) + j]);
                ix_s_set(ix, circ, k, s, 1);
                __encode(pool, obf->enc_vt, obf->shat[k][s][j], inps,
                         ix, obf->sp, &count_lock, &count, total);
            }
            ix = index_set_new(obf_params_nzs(circ));
            mpz_set_ui(inps[0], 1);
            mpz_set_ui(inps[1], 1);
            for (size_t p = 0; p < op->npowers; p++) {
                ix_s_set(ix, circ, k, s, 1 << p);
                __encode(pool, obf->enc_vt, obf->uhat[k][s][p], inps,
                         ix, obf->sp, &count_lock, &count, total);
            }
            for (size_t o = 0; o < noutputs; o++) {
                ix = index_set_new(obf_params_nzs(circ));
                if (k == 0)
                    ix_y_set(ix, circ, const_deg_max - const_deg[o]);
                for (size_t r = 0; r < acirc_symnum(circ, k); r++)
                    ix_s_set(ix, circ, k, r, r == s ? var_deg_max[k] - var_deg[k][o] : var_deg_max[k]);
                ix_z_set(ix, circ, k, 1);
                ix_w_set(ix, circ, k, 1);
                mpz_set(inps[0], delta[k][s][o]);
                mpz_set(inps[1], gamma[k][s][o]);
                __encode(pool, obf->enc_vt, obf->zhat[k][s][o], inps,
                         ix, obf->sp, &count_lock, &count, total);
                ix = index_set_new(obf_params_nzs(circ));
                ix_w_set(ix, circ, k, 1);
                mpz_set_ui(inps[0], 0);
                mpz_set   (inps[1], gamma[k][s][o]);
                __encode(pool, obf->enc_vt, obf->what[k][s][o], inps,
                         ix, obf->sp, &count_lock, &count, total);
            }
        }
    }

    for (size_t i = 0; i < acirc_nconsts(circ); i++) {
        ix = index_set_new(obf_params_nzs(circ));
        ix_y_set(ix, circ, 1);
        mpz_set_si(inps[0], acirc_const(circ, i));
        mpz_set   (inps[1], *beta[i]);
        __encode(pool, obf->enc_vt, obf->yhat[i], inps, ix,
                 obf->sp, &count_lock, &count, total);
    }
    for (size_t p = 0; p < op->npowers; p++) {
        ix = index_set_new(obf_params_nzs(circ));
        ix_y_set(ix, circ, 1 << p);
        mpz_set_ui(inps[0], 1);
        mpz_set_ui(inps[1], 1);
        __encode(pool, obf->enc_vt, obf->vhat[p], inps, ix,
                 obf->sp, &count_lock, &count, total);
    }

    {
        mpz_t **outputs;
        outputs = acirc_eval_mpz(circ, alpha, beta, moduli[1]);
        for (size_t o = 0; o < noutputs; ++o) {
            mpz_init_set(Cstar[o], *outputs[o]);
            mpz_clear(*outputs[o]);
            free(outputs[o]);
        }
        free(outputs);
    }

    for (size_t i = 0; i < noutputs; i++) {
        ix = index_set_new(obf_params_nzs(circ));
        ix_y_set(ix, circ, const_deg_max);
        for (size_t k = 0; k < nsymbols; k++) {
            for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
                ix_s_set(ix, circ, k, s, var_deg_max[k]);
            }
            ix_z_set(ix, circ, k, 1);
        }

        mpz_set_ui(inps[0], 0);
        mpz_set   (inps[1], Cstar[i]);

        __encode(pool, obf->enc_vt, obf->Chatstar[i], inps,
                 ix, obf->sp, &count_lock, &count, total);
    }

    if (nthreads)
        threadpool_destroy(pool);
    pthread_mutex_destroy(&count_lock);

    mpz_vect_clear(inps, 2);

    for (size_t k = 0; k < nsymbols; k++) {
        for (size_t j = 0; j < acirc_symlen(circ, k); j++) {
            mpz_vect_free(alpha[k * acirc_symlen(circ, k) + j], 1);
        }
    }
    free(alpha);

    for (size_t k = 0; k < nsymbols; k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            for (size_t o = 0; o < noutputs; o++) {
                mpz_clears(delta[k][s][o], gamma[k][s][o], NULL);
            }
        }
    }
    for (size_t i = 0; i < nconsts; i++)
        mpz_vect_free(beta[i], 1);
    free(beta);
    for (size_t i = 0; i < noutputs; i++)
        mpz_clear(Cstar[i]);
    free(const_deg);
    for (size_t i = 0; i < nsymbols; ++i)
        free(var_deg[i]);

    return obf;
}

static int
_fwrite(const obfuscation *const obf, FILE *const fp)
{
    const obf_params_t *const op = obf->op;

    const acirc_t *circ = op->circ;
    const size_t nconsts = acirc_nconsts(circ);
    const size_t noutputs = acirc_noutputs(circ);

    public_params_fwrite(obf->pp_vt, obf->pp, fp);
    for (size_t k = 0; k < acirc_nsymbols(circ); k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            for (size_t j = 0; j < acirc_symlen(circ, k); j++)
                encoding_fwrite(obf->enc_vt, obf->shat[k][s][j], fp);
            for (size_t p = 0; p < op->npowers; p++)
                encoding_fwrite(obf->enc_vt, obf->uhat[k][s][p], fp);
            for (size_t o = 0; o < noutputs; o++) {
                encoding_fwrite(obf->enc_vt, obf->zhat[k][s][o], fp);
                encoding_fwrite(obf->enc_vt, obf->what[k][s][o], fp);
            }
        }
    }
    for (size_t j = 0; j < nconsts; j++)
        encoding_fwrite(obf->enc_vt, obf->yhat[j], fp);
    for (size_t p = 0; p < op->npowers; p++)
        encoding_fwrite(obf->enc_vt, obf->vhat[p], fp);
    for (size_t k = 0; k < noutputs; k++)
        encoding_fwrite(obf->enc_vt, obf->Chatstar[k], fp);
    return OK;
}

static obfuscation *
_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    obfuscation *obf;

    const acirc_t *circ = op->circ;
    const size_t nconsts = acirc_nconsts(circ);
    const size_t noutputs = acirc_noutputs(circ);

    if ((obf = _alloc(mmap, op)) == NULL)
        return NULL;

    obf->pp = public_params_fread(obf->pp_vt, op, fp);
    for (size_t k = 0; k < acirc_nsymbols(circ); k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            for (size_t j = 0; j < acirc_symlen(circ, k); j++)
                obf->shat[k][s][j] = encoding_fread(obf->enc_vt, fp);
            for (size_t p = 0; p < op->npowers; p++)
                obf->uhat[k][s][p] = encoding_fread(obf->enc_vt, fp);
            for (size_t o = 0; o < noutputs; o++) {
                obf->zhat[k][s][o] = encoding_fread(obf->enc_vt, fp);
                obf->what[k][s][o] = encoding_fread(obf->enc_vt, fp);
            }
        }
    }
    for (size_t i = 0; i < nconsts; i++)
        obf->yhat[i] = encoding_fread(obf->enc_vt, fp);
    for (size_t p = 0; p < op->npowers; p++)
        obf->vhat[p] = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < noutputs; i++)
        obf->Chatstar[i] = encoding_fread(obf->enc_vt, fp);
    return obf;
}

static size_t g_max_npowers = 0;

static void
_raise_encoding(const obfuscation *obf, encoding *x, encoding **ys, size_t diff)
{
    while (diff > 0) {
        // want to find the largest power we obfuscated to multiply by
        size_t p = 0;
        while (((size_t) 1 << (p+1)) <= diff && (p+1) < obf->op->npowers)
            p++;
        if (g_max_npowers < p + 1)
            g_max_npowers = p + 1;
        encoding_mul(obf->enc_vt, obf->pp_vt, x, x, ys[p], obf->pp);
        diff -= (1 << p);
    }
}

static int
raise_encoding(const obfuscation *obf, encoding *x, const index_set *target)
{
    const acirc_t *circ = obf->op->circ;
    index_set *ix;
    size_t diff;

    if ((ix = index_set_difference(target, obf->enc_vt->mmap_set(x))) == NULL)
        return ERR;
    for (size_t k = 0; k < acirc_nsymbols(circ); k++) {
        for (size_t s = 0; s < acirc_symnum(circ, k); s++) {
            diff = ix_s_get(ix, circ, k, s);
            _raise_encoding(obf, x, obf->uhat[k][s], diff);
        }
    }
    diff = ix_y_get(ix, circ);
    _raise_encoding(obf, x, obf->vhat, diff);
    index_set_free(ix);
    return OK;
}

static int
raise_encodings(const obfuscation *obf, encoding *x, encoding *y)
{
    int ret = ERR;
    index_set *ix;

    ix = index_set_union(obf->enc_vt->mmap_set(x),
                         obf->enc_vt->mmap_set(y));
    if (raise_encoding(obf, x, ix) == ERR)
        goto cleanup;
    if (raise_encoding(obf, y, ix) == ERR)
        goto cleanup;
    ret = OK;
cleanup:
    index_set_free(ix);
    return ret;
}

typedef struct {
    const obfuscation *obf;
    const size_t *symbols;
    size_t *kappas;
} obf_args_t;

static void *
copy_f(void *x, void *args_)
{
    obf_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    encoding *out;

    out = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    encoding_set(obf->enc_vt, out, x);
    return out;
}

static void *
input_f(size_t ref, size_t i, void *args_)
{
    (void) ref;
    obf_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    const acirc_t *circ = obf->op->circ;
    const size_t slot = circ_params_slot(circ, i);
    const size_t sym = args->symbols[slot];
    const size_t bit = circ_params_bit(circ, i);
    return copy_f(obf->shat[slot][sym][bit], args_);
}

static void *
const_f(size_t ref, size_t i, long val, void *args_)
{
    (void) ref; (void) val;
    obf_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    return copy_f(obf->yhat[i], args_);
}

static void *
eval_f(size_t ref, acirc_op op, size_t xref, const void *x_, size_t yref, const void *y_, void *args_)
{
    (void) ref; (void) xref; (void) yref;
    const obf_args_t *const args = args_;
    const obfuscation *const obf = args->obf;
    const encoding *x = x_;
    const encoding *y = y_;
    encoding *res;

    res = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    switch (op) {
    case ACIRC_OP_MUL:
        encoding_mul(obf->enc_vt, obf->pp_vt, res, x, y, obf->pp);
        break;
    case ACIRC_OP_ADD:
    case ACIRC_OP_SUB: {
        encoding *tmp_x, *tmp_y;
        tmp_x = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        tmp_y = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        encoding_set(obf->enc_vt, tmp_x, x);
        encoding_set(obf->enc_vt, tmp_y, y);
        if (!index_set_eq(obf->enc_vt->mmap_set(tmp_x), obf->enc_vt->mmap_set(tmp_y)))
            raise_encodings(obf, tmp_x, tmp_y);
        if (op == ACIRC_OP_ADD) {
            encoding_add(obf->enc_vt, obf->pp_vt, res, tmp_x, tmp_y, obf->pp);
        } else if (op == ACIRC_OP_SUB) {
            encoding_sub(obf->enc_vt, obf->pp_vt, res, tmp_x, tmp_y, obf->pp);
        }
        encoding_free(obf->enc_vt, tmp_x);
        encoding_free(obf->enc_vt, tmp_y);
        break;
    }
    }
    return res;
}

static void *
output_f(size_t ref, size_t o, void *x, void *args_)
{
    (void) ref;
    long output = 1;
    const obf_args_t *args = args_;
    const size_t *symbols = args->symbols;
    const obfuscation *obf = args->obf;
    const acirc_t *circ = obf->op->circ;
    encoding *out, *lhs, *rhs, *tmp;
    const index_set *toplevel = obf->pp_vt->toplevel(obf->pp);

    out = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    lhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    rhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    tmp = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

    /* Compute LHS */
    encoding_set(obf->enc_vt, lhs, x);
    for (size_t k = 0; k < acirc_nsymbols(circ); k++) {
        encoding_set(obf->enc_vt, tmp, lhs);
        encoding_mul(obf->enc_vt, obf->pp_vt, lhs, tmp,
                     obf->zhat[k][symbols[k]][o], obf->pp);
    }
    if (raise_encoding(obf, lhs, toplevel) == ERR)
        goto cleanup;
    if (!index_set_eq(obf->enc_vt->mmap_set(lhs), toplevel)) {
        fprintf(stderr, "%s: %s: lhs != toplevel\n", errorstr,
                __func__);
        index_set_print(obf->enc_vt->mmap_set(lhs));
        index_set_print(toplevel);
        goto cleanup;
    }

    /* Compute RHS */
    encoding_set(obf->enc_vt, rhs, obf->Chatstar[o]);
    for (size_t k = 0; k < acirc_nsymbols(circ); k++) {
        encoding_set(obf->enc_vt, tmp, rhs);
        encoding_mul(obf->enc_vt, obf->pp_vt, rhs, tmp,
                     obf->what[k][symbols[k]][o], obf->pp);
    }
    if (!index_set_eq(obf->enc_vt->mmap_set(rhs), toplevel)) {
        fprintf(stderr, "%s: %s: rhs != toplevel\n", errorstr,
                __func__);
        index_set_print(obf->enc_vt->mmap_set(rhs));
        index_set_print(toplevel);
        goto cleanup;
    }
    encoding_sub(obf->enc_vt, obf->pp_vt, out, lhs, rhs, obf->pp);
    output = !encoding_is_zero(obf->enc_vt, obf->pp_vt, out, obf->pp);
    if (args->kappas)
        args->kappas[o] = encoding_get_degree(obf->enc_vt, out);

cleanup:
    encoding_free(obf->enc_vt, out);
    encoding_free(obf->enc_vt, lhs);
    encoding_free(obf->enc_vt, rhs);
    encoding_free(obf->enc_vt, tmp);
    return (void *) output;
}

static void
free_f(void *x, void *args_)
{
    obf_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    if (x)
        encoding_free(obf->enc_vt, x);
}

static int
_evaluate(const obfuscation *obf, long *outputs, size_t noutputs,
          const long *inputs, size_t ninputs, size_t nthreads,
          size_t *kappa, size_t *npowers)
{
    const acirc_t *circ = obf->op->circ;
    size_t *kappas = NULL;
    size_t *symbols;
    int ret = ERR;

    if (ninputs != acirc_ninputs(circ)) {
        fprintf(stderr, "%s: %s: invalid number of inputs\n",
                errorstr, __func__);
        return ERR;
    }
    if (noutputs != acirc_noutputs(circ)) {
        fprintf(stderr, "%s: %s: invalid number of outputs\n",
                errorstr, __func__);
        return ERR;
    }

    g_max_npowers = 0;

    if (kappa)
        kappas = my_calloc(acirc_noutputs(circ), sizeof kappas[0]);
    if ((symbols = get_input_syms(inputs, ninputs, circ)) == NULL)
        goto finish;

    {
        long *tmp;
        obf_args_t args = {
            .obf = obf,
            .symbols = symbols,
            .kappas = kappas,
        };
        tmp = (long *) acirc_traverse((acirc_t *) circ, input_f, const_f, eval_f, output_f,
                                      free_f, NULL, NULL, &args, nthreads);
        if (outputs)
            for (size_t i = 0; i < acirc_noutputs(circ); ++i)
                outputs[i] = tmp[i];
        free(tmp);
    }
    ret = OK;

    if (kappas) {
        unsigned int maxkappa = 0;
        for (size_t i = 0; i < noutputs; i++) {
            if (kappas[i] > maxkappa)
                maxkappa = kappas[i];
        }
        *kappa = maxkappa;
    }
    if (npowers)
        *npowers = g_max_npowers;
finish:
    if (kappas)
        free(kappas);
    if (symbols)
        free(symbols);
    return ret;
}

obfuscator_vtable lz_obfuscator_vtable = {
    .free = _free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _fwrite,
    .fread = _fread,
};
