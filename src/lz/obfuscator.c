#include "obfuscator.h"
#include "obf_index.h"
#include "obf_params.h"
#include "vtables.h"
#include "../reflist.h"
#include "../util.h"

#include <assert.h>
#include <string.h>
#include <threadpool.h>

typedef struct obfuscation {
    const mmap_vtable *mmap;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    const encoding_vtable *enc_vt;
    const obf_params_t *op;
    secret_params *sp;
    public_params *pp;
    aes_randstate_t rng;
    encoding ****shat;          // [c][Σ][ℓ]
    encoding ****uhat;          // [c][Σ][npowers]
    encoding ****zhat;          // [c][Σ][γ]
    encoding ****what;          // [c][Σ][γ]
    encoding **yhat;            // [m]
    encoding **vhat;            // [npowers]
    encoding **Chatstar;        // [γ]
} obfuscation;

typedef struct work_args {
    const mmap_vtable *mmap;
    acircref ref;
    const acirc *c;
    const int *inputs;
    const obfuscation *obf;
    bool *mine;
    int *ready;
    void *cache;
    ref_list **deps;
    threadpool *pool;
    unsigned int *degrees;
    int *rop;
} work_args;

static size_t
num_encodings(const obf_params_t *op)
{
    return op->c * op->q * op->ell
        + op->c * op->q * op->npowers
        + op->c * op->q * op->gamma
        + op->c * op->q * op->gamma
        + op->m
        + op->npowers
        + op->gamma;
}

typedef struct obf_args {
    const encoding_vtable *vt;
    encoding *enc;
    mpz_t inps[2];
    obf_index *ix;
    const secret_params *sp;
    pthread_mutex_t *count_lock;
    size_t *count;
    size_t total;
} obf_args;

static void obf_worker(void *wargs)
{
    obf_args *const args = wargs;

    encode(args->vt, args->enc, args->inps, 2, args->ix, args->sp);
    if (g_verbose) {
        pthread_mutex_lock(args->count_lock);
        print_progress(++*args->count, args->total);
        pthread_mutex_unlock(args->count_lock);
    }
    mpz_vect_clear(args->inps, 2);
    obf_index_free(args->ix);
    free(args);
}

static void
__encode(threadpool *pool, const encoding_vtable *vt, encoding *enc, mpz_t inps[2],
         obf_index *ix, const secret_params *sp, pthread_mutex_t *count_lock,
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

static void
_obfuscator_free(obfuscation *obf);

static obfuscation *
__obfuscator_new(const mmap_vtable *mmap, const obf_params_t *op)
{
    obfuscation *obf;

    obf = my_calloc(1, sizeof obf[0]);
    obf->mmap = mmap;
    obf->enc_vt = get_encoding_vtable(mmap);
    obf->pp_vt = get_pp_vtable(mmap);
    obf->sp_vt = get_sp_vtable(mmap);
    obf->op = op;
    obf->shat = my_calloc(op->c, sizeof obf->shat[0]);
    obf->uhat = my_calloc(op->c, sizeof obf->uhat[0]);
    obf->zhat = my_calloc(op->c, sizeof obf->zhat[0]);
    obf->what = my_calloc(op->c, sizeof obf->what[0]);
    for (size_t k = 0; k < op->c; k++) {
        obf->shat[k] = my_calloc(op->q, sizeof obf->shat[0][0]);
        obf->uhat[k] = my_calloc(op->q, sizeof obf->uhat[0][0]);
        obf->zhat[k] = my_calloc(op->q, sizeof obf->zhat[0][0]);
        obf->what[k] = my_calloc(op->q, sizeof obf->what[0][0]);
        for (size_t s = 0; s < op->q; s++) {
            obf->shat[k][s] = my_calloc(op->ell, sizeof obf->shat[0][0][0]);
            obf->uhat[k][s] = my_calloc(op->npowers, sizeof obf->uhat[0][0][0]);
            obf->zhat[k][s] = my_calloc(op->gamma, sizeof obf->zhat[0][0][0]);
            obf->what[k][s] = my_calloc(op->gamma, sizeof obf->what[0][0][0]);
        }
    }
    obf->yhat = my_calloc(op->m, sizeof obf->yhat[0]);
    obf->vhat = my_calloc(op->npowers, sizeof obf->vhat[0]);
    obf->Chatstar = my_calloc(op->gamma, sizeof obf->Chatstar[0]);
    return obf;
}

static obfuscation *
_obfuscator_new(const mmap_vtable *mmap, const obf_params_t *op,
                size_t secparam, size_t kappa)
{
    obfuscation *obf;

    obf = __obfuscator_new(mmap, op);
    aes_randinit(obf->rng);
    obf->sp = secret_params_new(obf->sp_vt, op, secparam, kappa, obf->rng);
    if (obf->sp == NULL)
        goto error;
    obf->pp = public_params_new(obf->pp_vt, obf->sp_vt, obf->sp);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                obf->shat[k][s][j] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
            for (size_t p = 0; p < op->npowers; p++) {
                obf->uhat[k][s][p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
            for (size_t o = 0; o < op->gamma; o++) {
                obf->zhat[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
                obf->what[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }
    for (size_t i = 0; i < op->m; i++) {
        obf->yhat[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    for (size_t p = 0; p < op->npowers; p++) {
        obf->vhat[p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Chatstar[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    return obf;

error:
     _obfuscator_free(obf);
    return NULL;
}

static void
_obfuscator_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const obf_params_t *op = obf->op;

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++)
                encoding_free(obf->enc_vt, obf->shat[k][s][j]);
            for (size_t p = 0; p < op->npowers; p++)
                encoding_free(obf->enc_vt, obf->uhat[k][s][p]);
            for (size_t o = 0; o < op->gamma; o++) {
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
    for (size_t i = 0; i < op->m; i++)
        encoding_free(obf->enc_vt, obf->yhat[i]);
    free(obf->yhat);
    for (size_t p = 0; p < op->npowers; p++)
        encoding_free(obf->enc_vt, obf->vhat[p]);
    free(obf->vhat);
    for (size_t i = 0; i < op->gamma; i++)
        encoding_free(obf->enc_vt, obf->Chatstar[i]);
    free(obf->Chatstar);

    if (obf->pp)
        public_params_free(obf->pp_vt, obf->pp);
    if (obf->sp)
        secret_params_free(obf->sp_vt, obf->sp);

    aes_randclear(obf->rng);

    free(obf);
}

static int
_obfuscate(obfuscation *obf, size_t nthreads)
{
    const obf_params_t *const op = obf->op;
    acirc *const c = op->circ;
    mpz_t *const moduli =
        mpz_vect_create_of_fmpz(obf->mmap->sk->plaintext_fields(obf->sp->sk),
                                obf->mmap->sk->nslots(obf->sp->sk));

    obf_index *const ix = obf_index_new(op);

    mpz_t inps[2];
    mpz_t alpha[op->c * op->ell];
    mpz_t beta[op->m];
    mpz_t gamma[op->c][op->q][op->gamma];
    mpz_t delta[op->c][op->q][op->gamma];
    mpz_t Cstar[op->gamma];

    threadpool *pool = threadpool_create(nthreads);

    pthread_mutex_t count_lock;
    size_t count = 0;
    const size_t total = num_encodings(op);

    pthread_mutex_init(&count_lock, NULL);
    mpz_vect_init(inps, 2);

    assert(obf->mmap->sk->nslots(obf->sp->sk) >= 2);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t j = 0; j < op->ell; j++) {
            mpz_init(alpha[k * op->ell + j]);
            mpz_randomm_inv(alpha[k * op->ell + j], obf->rng, moduli[1]);
        }
    }
    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t o = 0; o < op->gamma; o++) {
                mpz_inits(gamma[k][s][o], delta[k][s][o], NULL);
                mpz_randomm_inv(delta[k][s][o], obf->rng, moduli[0]);
                mpz_randomm_inv(gamma[k][s][o], obf->rng, moduli[1]);
            }
        }
    }
    for (size_t i = 0; i < op->m; i++) {
        mpz_init(beta[i]);
        mpz_randomm_inv(beta[i], obf->rng, moduli[1]);
    }

    for (size_t o = 0; o < op->gamma; o++) {
        mpz_init(Cstar[o]);
        acirc_eval_mpz_mod(Cstar[o], c, c->outputs.buf[o], alpha, beta, moduli[1]);
    }

    unsigned long const_deg[op->gamma];
    unsigned long const_deg_max = 0;
    unsigned long var_deg[op->c][op->gamma];
    unsigned long var_deg_max[op->c];
    acirc_memo *memo;

    memset(const_deg, '\0', sizeof const_deg);
    memset(var_deg, '\0', sizeof var_deg);
    memset(var_deg_max, '\0', sizeof var_deg_max);

    memo = acirc_memo_new(c);
    for (size_t o = 0; o < op->gamma; o++) {
        const_deg[o] = acirc_const_degree(c, c->outputs.buf[o], memo);
        if (const_deg[o] > const_deg_max)
            const_deg_max = const_deg[o];
    }
    acirc_memo_free(memo, c);

    memo = acirc_memo_new(c);
    for (size_t k = 0; k < op->c; k++) {
        for (size_t o = 0; o < op->gamma; o++) {
            var_deg[k][o] = acirc_var_degree(c, c->outputs.buf[o], k, memo);
            if (var_deg[k][o] > var_deg_max[k])
                var_deg_max[k] = var_deg[k][o];
        }
    }
    acirc_memo_free(memo, c);

    if (g_verbose) {
        print_progress(count, total);
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                mpz_set_ui(inps[0], op->sigma ? s == j : bit(s, j));
                mpz_set   (inps[1], alpha[k * op->ell + j]);
                obf_index_clear(ix);
                IX_S(ix, op, k, s) = 1;
                __encode(pool, obf->enc_vt, obf->shat[k][s][j], inps,
                         obf_index_copy(ix, op), obf->sp, &count_lock, &count,
                         total);
            }
            mpz_set_ui(inps[0], 1);
            mpz_set_ui(inps[1], 1);
            obf_index_clear(ix);
            for (size_t p = 0; p < op->npowers; p++) {
                IX_S(ix, op, k, s) = 1 << p;
                __encode(pool, obf->enc_vt, obf->uhat[k][s][p], inps,
                         obf_index_copy(ix, op), obf->sp, &count_lock, &count,
                         total);
            }
            for (size_t o = 0; o < op->gamma; o++) {
                obf_index_clear(ix);
                if (k == 0)
                    IX_Y(ix) = const_deg_max - const_deg[o];
                for (size_t r = 0; r < op->q; r++)
                    IX_S(ix, op, k, r) = (r == s ? var_deg_max[k] - var_deg[k][o] : var_deg_max[k]);
                IX_Z(ix, op, k) = 1;
                IX_W(ix, op, k) = 1;
                mpz_set(inps[0], delta[k][s][o]);
                mpz_set(inps[1], gamma[k][s][o]);
                __encode(pool, obf->enc_vt, obf->zhat[k][s][o], inps,
                         obf_index_copy(ix, op), obf->sp, &count_lock, &count,
                         total);

                obf_index_clear(ix);
                IX_W(ix, op, k) = 1;
                mpz_set_ui(inps[0], 0);
                mpz_set   (inps[1], gamma[k][s][o]);
                __encode(pool, obf->enc_vt, obf->what[k][s][o], inps,
                         obf_index_copy(ix, op), obf->sp, &count_lock, &count,
                         total);
            }
        }
    }

    for (size_t i = 0; i < op->m; i++) {
        obf_index_clear(ix);
        IX_Y(ix) = 1;
        mpz_set_ui(inps[0], c->consts.buf[i]);
        mpz_set   (inps[1], beta[i]);
        __encode(pool, obf->enc_vt, obf->yhat[i], inps, obf_index_copy(ix, op),
                 obf->sp, &count_lock, &count, total);
    }
    for (size_t p = 0; p < op->npowers; p++) {
        obf_index_clear(ix);
        IX_Y(ix) = 1 << p;
        mpz_set_ui(inps[0], 1);
        mpz_set_ui(inps[1], 1);
        __encode(pool, obf->enc_vt, obf->vhat[p], inps, obf_index_copy(ix, op),
                 obf->sp, &count_lock, &count, total);
    }

    for (size_t i = 0; i < op->gamma; i++) {
        obf_index_clear(ix);
        IX_Y(ix) = const_deg_max;
        for (size_t k = 0; k < op->c; k++) {
            for (size_t s = 0; s < op->q; s++) {
                IX_S(ix, op, k, s) = var_deg_max[k];
            }
            IX_Z(ix, op, k) = 1;
        }

        mpz_set_ui(inps[0], 0);
        mpz_set   (inps[1], Cstar[i]);

        __encode(pool, obf->enc_vt, obf->Chatstar[i], inps,
                 obf_index_copy(ix, op), obf->sp, &count_lock, &count, total);
    }

    threadpool_destroy(pool);
    pthread_mutex_destroy(&count_lock);

    obf_index_free(ix);
    mpz_vect_clear(inps, 2);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t j = 0; j < op->ell; j++) {
            mpz_clear(alpha[k * op->ell + j]);
        }
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t o = 0; o < op->gamma; o++) {
                mpz_clears(delta[k][s][o], gamma[k][s][o], NULL);
            }
        }
    }
    for (size_t i = 0; i < op->m; i++)
        mpz_clear(beta[i]);
    for (size_t i = 0; i < op->gamma; i++)
        mpz_clear(Cstar[i]);

    mpz_vect_free(moduli, obf->mmap->sk->nslots(obf->sp->sk));

    return OK;
}

static int
_obfuscator_fwrite(const obfuscation *const obf, FILE *const fp)
{
    const obf_params_t *const op = obf->op;

    public_params_fwrite(obf->pp_vt, obf->pp, fp);
    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++)
                encoding_fwrite(obf->enc_vt, obf->shat[k][s][j], fp);
            for (size_t p = 0; p < op->npowers; p++)
                encoding_fwrite(obf->enc_vt, obf->uhat[k][s][p], fp);
            for (size_t o = 0; o < op->gamma; o++) {
                encoding_fwrite(obf->enc_vt, obf->zhat[k][s][o], fp);
                encoding_fwrite(obf->enc_vt, obf->what[k][s][o], fp);
            }
        }
    }
    for (size_t j = 0; j < op->m; j++)
        encoding_fwrite(obf->enc_vt, obf->yhat[j], fp);
    for (size_t p = 0; p < op->npowers; p++)
        encoding_fwrite(obf->enc_vt, obf->vhat[p], fp);
    for (size_t k = 0; k < op->gamma; k++)
        encoding_fwrite(obf->enc_vt, obf->Chatstar[k], fp);
    return OK;
}

static obfuscation *
_obfuscator_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    obfuscation *obf;

    if ((obf = __obfuscator_new(mmap, op)) == NULL)
        return NULL;

    obf->pp = public_params_fread(obf->pp_vt, op, fp);
    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++)
                obf->shat[k][s][j] = encoding_fread(obf->enc_vt, fp);
            for (size_t p = 0; p < op->npowers; p++)
                obf->uhat[k][s][p] = encoding_fread(obf->enc_vt, fp);
            for (size_t o = 0; o < op->gamma; o++) {
                obf->zhat[k][s][o] = encoding_fread(obf->enc_vt, fp);
                obf->what[k][s][o] = encoding_fread(obf->enc_vt, fp);
            }
        }
    }
    for (size_t i = 0; i < op->m; i++)
        obf->yhat[i] = encoding_fread(obf->enc_vt, fp);
    for (size_t p = 0; p < op->npowers; p++)
        obf->vhat[p] = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < op->gamma; i++)
        obf->Chatstar[i] = encoding_fread(obf->enc_vt, fp);
    return obf;
}

static void _raise_encoding(const obfuscation *obf, encoding *x, encoding **ys, size_t diff)
{
    while (diff > 0) {
        // want to find the largest power we obfuscated to multiply by
        size_t p = 0;
        while (((size_t) 1 << (p+1)) <= diff && (p+1) < obf->op->npowers)
            p++;
        encoding_mul(obf->enc_vt, obf->pp_vt, x, x, ys[p], obf->pp);
        diff -= (1 << p);
    }
}

static void raise_encoding(const obfuscation *obf, encoding *x, const obf_index *target)
{
    obf_index *const ix =
        obf_index_difference(obf->op, target, obf->enc_vt->mmap_set(x));
    size_t diff;
    for (size_t k = 0; k < obf->op->c; k++) {
        for (size_t s = 0; s < obf->op->q; s++) {
            diff = IX_S(ix, obf->op, k, s);
            _raise_encoding(obf, x, obf->uhat[k][s], diff);
        }
    }
    diff = IX_Y(ix);
    _raise_encoding(obf, x, obf->vhat, diff);
    obf_index_free(ix);
}

static void
raise_encodings(const obfuscation *obf, encoding *x, encoding *y)
{
    obf_index *const ix = obf_index_union(obf->op, obf->enc_vt->mmap_set(x),
                                          obf->enc_vt->mmap_set(y));
    raise_encoding(obf, x, ix);
    raise_encoding(obf, y, ix);
    obf_index_free(ix);
}

static void eval_worker(void *vargs)
{
    work_args *const wargs = vargs;
    const acircref ref = wargs->ref;
    const acirc *const c = wargs->c;
    const int *const inputs = wargs->inputs;
    const obfuscation *const obf = wargs->obf;
    bool *const mine = wargs->mine;
    int *const ready = wargs->ready;
    encoding **cache = wargs->cache;
    ref_list *const *const deps = wargs->deps;
    threadpool *const pool = wargs->pool;
    unsigned int *const degrees = wargs->degrees;
    int *const rop = wargs->rop;

    const acirc_operation op = c->gates.gates[ref].op;
    const acircref *const args = c->gates.gates[ref].args;
    encoding *res;

    switch (op) {
    case OP_INPUT: {
        const size_t id = args[0];
        const sym_id sym = obf->op->chunker(id, c->ninputs, obf->op->c);
        const size_t k = sym.sym_number;
        const size_t s = inputs[k];
        const size_t j = sym.bit_number;
        res = obf->shat[k][s][j];
        mine[ref] = false;
        break;
    }
    case OP_CONST: {
        const size_t i = args[0];
        res = obf->yhat[i];
        mine[ref] = false;
        break;
    }
    case OP_ADD: case OP_SUB: case OP_MUL: {
        assert(c->gates.gates[ref].nargs == 2);
        res = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        mine[ref] = true;

        // the encodings of the args exist since the ref's children signalled it
        const encoding *const x = cache[args[0]];
        const encoding *const y = cache[args[1]];

        /* printf("%lu %lu %lu\n", args[0], args[1], ref); */

        if (op == OP_MUL) {
            encoding_mul(obf->enc_vt, obf->pp_vt, res, x, y, obf->pp);
        } else {
            encoding *tmp_x, *tmp_y;
            tmp_x = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            tmp_y = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            encoding_set(obf->enc_vt, tmp_x, x);
            encoding_set(obf->enc_vt, tmp_y, y);
            if (!obf_index_eq(obf->enc_vt->mmap_set(tmp_x), obf->enc_vt->mmap_set(tmp_y)))
                raise_encodings(obf, tmp_x, tmp_y);
            if (op == OP_ADD) {
                encoding_add(obf->enc_vt, obf->pp_vt, res, tmp_x, tmp_y, obf->pp);
            } else if (op == OP_SUB) {
                encoding_sub(obf->enc_vt, obf->pp_vt, res, tmp_x, tmp_y, obf->pp);
            } else {
                abort();
            }
            encoding_free(obf->enc_vt, tmp_x);
            encoding_free(obf->enc_vt, tmp_y);
        }
        break;
    }
    case OP_SET:
        res = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        mine[ref] = true;
        encoding_set(obf->enc_vt, res, cache[args[0]]);
        break;
    default:
        fprintf(stderr, "fatal: op not supported\n");
        abort();
    }        

    cache[ref] = res;

    // signal parents that this ref is done
    ref_list_node *cur = deps[ref]->first;
    while (cur != NULL) {
        const int num = __sync_add_and_fetch(&ready[cur->ref], 1);
        if (num == 2) {
            work_args *newargs = my_calloc(1, sizeof newargs[0]);
            memcpy(newargs, wargs, sizeof newargs[0]);
            newargs->ref = cur->ref;
            threadpool_add_job(pool, eval_worker, newargs);
        } else {
            cur = cur->next;
        }
    }

    free(wargs);

    // addendum: is this ref an output bit? if so, we should zero test it.
    ssize_t output = -1;
    for (size_t i = 0; i < obf->op->gamma; i++) {
        if (ref == c->outputs.buf[i]) {
            output = i;
            break;
        }
    }

    if (output != -1) {
        encoding *out, *lhs, *rhs, *tmp, *tmp2;
        const obf_index *const toplevel = obf->pp_vt->toplevel(obf->pp);

        out = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        lhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        rhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        tmp = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        tmp2 = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

        /* Compute RHS */
        encoding_set(obf->enc_vt, tmp, obf->Chatstar[output]);
        for (size_t k = 0; k < obf->op->c; k++) {
            encoding_set(obf->enc_vt, tmp2, tmp);
            encoding_mul(obf->enc_vt, obf->pp_vt, tmp, tmp2,
                         obf->what[k][inputs[k]][output], obf->pp);
        }
        encoding_set(obf->enc_vt, rhs, tmp);
        if (!obf_index_eq(obf->enc_vt->mmap_set(rhs), toplevel)) {
            fprintf(stderr, "rhs != toplevel\n");
            obf_index_print(obf->enc_vt->mmap_set(rhs));
            obf_index_print(toplevel);
            rop[output] = 1;
            goto cleanup;
        }
        /* Compute LHS */
        encoding_set(obf->enc_vt, tmp, res);
        for (size_t k = 0; k < obf->op->c; k++) {
            encoding_set(obf->enc_vt, tmp2, tmp);
            encoding_mul(obf->enc_vt, obf->pp_vt, tmp, tmp2,
                         obf->zhat[k][inputs[k]][output], obf->pp);
        }
        raise_encoding(obf, tmp, toplevel);
        encoding_set(obf->enc_vt, lhs, tmp);
        if (!obf_index_eq(obf->enc_vt->mmap_set(lhs), toplevel)) {
            fprintf(stderr, "lhs != toplevel\n");
            obf_index_print(obf->enc_vt->mmap_set(lhs));
            obf_index_print(toplevel);
            rop[output] = 1;
            goto cleanup;
        }

        encoding_sub(obf->enc_vt, obf->pp_vt, out, lhs, rhs, obf->pp);
        degrees[output] = encoding_get_degree(obf->enc_vt, out);
        rop[output] = !encoding_is_zero(obf->enc_vt, obf->pp_vt, out, obf->pp);

    cleanup:
        encoding_free(obf->enc_vt, out);
        encoding_free(obf->enc_vt, lhs);
        encoding_free(obf->enc_vt, rhs);
        encoding_free(obf->enc_vt, tmp);
        encoding_free(obf->enc_vt, tmp2);
    }
}


static int
_evaluate(int *rop, const int *inputs, const obfuscation *obf, size_t nthreads,
          unsigned int *degree)
{
    const acirc *const c = obf->op->circ;

    encoding **cache = my_calloc(acirc_nrefs(c), sizeof cache[0]);
    bool *mine = my_calloc(acirc_nrefs(c), sizeof mine[0]);
    int *ready = my_calloc(acirc_nrefs(c), sizeof ready[0]);
    unsigned int *degrees = my_calloc(c->outputs.n, sizeof degrees[0]);
    int *input_syms = get_input_syms(inputs, c->ninputs, obf->op->rchunker,
                                     obf->op->c, obf->op->ell, obf->op->q,
                                     obf->op->sigma);
    ref_list **deps = ref_lists_new(c);

    threadpool *pool = threadpool_create(nthreads);

    // start threads evaluating the circuit inputs- they will signal their
    // parents to start, recursively, until the output is reached.
    for (size_t ref = 0; ref < acirc_nrefs(c); ref++) {
        acirc_operation op = c->gates.gates[ref].op;
        if (!(op == OP_INPUT || op == OP_CONST))
            continue;
        // allocate each argstruct here, otherwise we will overwrite
        // it each time we add to the job list. The worker will free.
        work_args *args = calloc(1, sizeof(work_args));
        args->mmap   = obf->mmap;
        args->ref    = ref;
        args->c      = c;
        args->inputs = input_syms;
        args->obf    = obf;
        args->mine   = mine;
        args->ready  = ready;
        args->cache  = cache;
        args->deps   = deps;
        args->pool   = pool;
        args->rop    = rop;
        args->degrees = degrees;
        threadpool_add_job(pool, eval_worker, args);
    }

    threadpool_destroy(pool);

    unsigned int maxdeg = 0;
    for (size_t i = 0; i < c->outputs.n; i++) {
        if (degrees[i] > maxdeg)
            maxdeg = degrees[i];
    }
    if (degree)
        *degree = maxdeg;

    for (size_t i = 0; i < acirc_nrefs(c); i++) {
        if (mine[i]) {
            encoding_free(obf->enc_vt, cache[i]);
        }
    }
    ref_lists_free(deps, c);
    free(cache);
    free(mine);
    free(ready);
    free(degrees);
    free(input_syms);

    return OK;
}

obfuscator_vtable lz_obfuscator_vtable = {
    .new =  _obfuscator_new,
    .free = _obfuscator_free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _obfuscator_fwrite,
    .fread = _obfuscator_fread,
};
