#include "obfuscator.h"
#include "obf_params.h"

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

static void
my_encoding_print(const char *name, mpz_t inps[2], obf_index *ix)
{
    if (LOG_INFO) {
        gmp_fprintf(stderr,
                    "=======================\n"
                    "\t%s\n"
                    "\t%Zd\t%Zd\n\t", name, inps[0], inps[1]);
        obf_index_print(ix);
        fprintf(stderr, "=======================\n");
    }
}

typedef struct obf_args {
    const encoding_vtable *vt;
    encoding *enc;
    mpz_t inps[2];
    obf_index *ix;
    const secret_params *sp;
} obf_args;

static void obf_worker(void *wargs)
{
    const obf_args *const args = wargs;

    encode(args->vt, args->enc, args->inps, 2, args->ix, args->sp);
    mpz_vect_clear(args->inps, 2);
    obf_index_free(args->ix);
    free(args);
}

static void
__encode(threadpool *pool, const encoding_vtable *vt, encoding *enc, mpz_t inps[2],
         obf_index *ix, const secret_params *sp)
{
    obf_args *args = my_calloc(1, sizeof(obf_args));
    args->vt = vt;
    args->enc = enc;
    mpz_vect_init(args->inps, 2);
    mpz_set(args->inps[0], inps[0]);
    mpz_set(args->inps[1], inps[1]);
    args->ix = ix;
    args->sp = sp;
    threadpool_add_job(pool, obf_worker, args);
}

static void
_obfuscator_free(obfuscation *obf);

static obfuscation *
_obfuscator_new(const mmap_vtable *mmap, const obf_params_t *op,
                size_t secparam, size_t kappa)
{
    obfuscation *obf;

    obf = my_calloc(1, sizeof(obfuscation));
    obf->mmap = mmap;
    obf->enc_vt = get_encoding_vtable(mmap);
    obf->pp_vt = get_pp_vtable(mmap);
    obf->sp_vt = get_sp_vtable(mmap);
    obf->op = op;
    aes_randinit(obf->rng);
    obf->sp = secret_params_new(obf->sp_vt, op, secparam, kappa, obf->rng);
    obf->pp = public_params_new(obf->pp_vt, obf->sp_vt, obf->sp);

    obf->shat = my_calloc(op->c, sizeof(encoding ***));
    obf->uhat = my_calloc(op->c, sizeof(encoding ***));
    obf->zhat = my_calloc(op->c, sizeof(encoding ***));
    obf->what = my_calloc(op->c, sizeof(encoding ***));
    for (size_t k = 0; k < op->c; k++) {
        obf->shat[k] = my_calloc(op->q, sizeof(encoding **));
        obf->uhat[k] = my_calloc(op->q, sizeof(encoding **));
        obf->zhat[k] = my_calloc(op->q, sizeof(encoding **));
        obf->what[k] = my_calloc(op->q, sizeof(encoding **));
        for (size_t s = 0; s < op->q; s++) {
            obf->shat[k][s] = my_calloc(op->ell, sizeof(encoding *));
            for (size_t j = 0; j < op->ell; j++) {
                obf->shat[k][s][j] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
            obf->uhat[k][s] = my_calloc(op->npowers, sizeof(encoding *));
            for (size_t p = 0; p < op->npowers; p++) {
                obf->uhat[k][s][p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
            obf->zhat[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            obf->what[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            for (size_t o = 0; o < op->gamma; o++) {
                obf->zhat[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
                obf->what[k][s][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }
    obf->yhat = my_calloc(op->m, sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->yhat[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    obf->vhat = my_calloc(op->npowers, sizeof(encoding *));
    for (size_t p = 0; p < op->npowers; p++) {
        obf->vhat[p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    obf->Chatstar = my_calloc(op->gamma, sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Chatstar[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    return obf;
}

static void
_obfuscator_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const obf_params_t *op = obf->op;

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                encoding_free(obf->enc_vt, obf->shat[k][s][j]);
            }
            for (size_t p = 0; p < op->npowers; p++) {
                encoding_free(obf->enc_vt, obf->uhat[k][s][p]);
            }

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
    for (size_t i = 0; i < op->m; i++) {
        encoding_free(obf->enc_vt, obf->yhat[i]);
    }
    free(obf->yhat);
    for (size_t p = 0; p < op->npowers; p++) {
        encoding_free(obf->enc_vt, obf->vhat[p]);
    }
    free(obf->vhat);
    for (size_t i = 0; i < op->gamma; i++) {
        encoding_free(obf->enc_vt, obf->Chatstar[i]);
    }
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
    mpz_t alpha[op->c][op->ell];
    mpz_t beta[op->m];
    mpz_t gamma[op->c][op->q][op->gamma];
    mpz_t delta[op->c][op->q][op->gamma];
    mpz_t Cstar[op->gamma];

    threadpool *pool = threadpool_create(nthreads);

    size_t count = 0;
    const size_t total = num_encodings(op);

    mpz_vect_init(inps, 2);

    if (LOG_DEBUG)
        gmp_printf("%Zd\t%Zd\n", moduli[0], moduli[1]);

    assert(obf->mmap->sk->nslots(obf->sp->sk) >= 2);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t j = 0; j < op->ell; j++) {
            mpz_init(alpha[k][j]);
            mpz_randomm_inv(alpha[k][j], obf->rng, moduli[1]);
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
    for (size_t i = 0; i < op->gamma; i++) {
        mpz_init(Cstar[i]);
        acirc_eval_mpz_mod(Cstar[i], c, c->outrefs[i], alpha, beta, moduli[1]);
    }

    unsigned long const_deg[op->gamma];
    unsigned long const_deg_max = 0;
    unsigned long var_deg[op->c][op->gamma];
    unsigned long var_deg_max[op->c];

    memset(const_deg, '\0', sizeof const_deg);
    memset(var_deg, '\0', sizeof var_deg);
    memset(var_deg_max, '\0', sizeof var_deg_max);

    for (size_t o = 0; o < op->gamma; o++) {
        const_deg[o] = acirc_const_degree(c, c->outrefs[o]);
        if (const_deg[o] > const_deg_max)
            const_deg_max = const_deg[o];
    }

    for (size_t k = 0; k < op->c; k++) {
        for (size_t o = 0; o < op->gamma; o++) {
            var_deg[k][o] = acirc_var_degree(c, c->outrefs[o], k);
            if (var_deg[k][o] > var_deg_max[k])
                var_deg_max[k] = var_deg[k][o];
        }
    }
    
    printf("Encoding:\n");
    print_progress(count, total);
    char tmp[1024];

    for (size_t k = 0; k < op->c; k++) {
        for (size_t s = 0; s < op->q; s++) {
            for (size_t j = 0; j < op->ell; j++) {
                if (op->rachel_inputs)
                    mpz_set_ui(inps[0], s == j);
                else
                    mpz_set_ui(inps[0], bit(s, j));
                mpz_set   (inps[1], alpha[k][j]);

                obf_index_clear(ix);
                IX_S(ix, op, k, s) = 1;
                sprintf(tmp, "shat[%lu,%lu,%lu]", k, s, j);
                my_encoding_print(tmp, inps, ix);
                __encode(pool, obf->enc_vt, obf->shat[k][s][j], inps, obf_index_copy(ix, op), obf->sp);
                {
                    print_progress(++count, total);
                }
            }
            mpz_set_ui(inps[0], 1);
            mpz_set_ui(inps[1], 1);
            obf_index_clear(ix);
            for (size_t p = 0; p < op->npowers; p++) {
                IX_S(ix, op, k, s) = 1 << p;
                sprintf(tmp, "uhat[%lu,%lu,%lu]", k, s, p);
                my_encoding_print(tmp, inps, ix);
                __encode(pool, obf->enc_vt, obf->uhat[k][s][p], inps, obf_index_copy(ix, op), obf->sp);
                {
                    print_progress(++count, total);
                }
            }


            for (size_t o = 0; o < op->gamma; o++) {
                obf_index_clear(ix);
                if (k == 0) {
                    IX_Y(ix) = const_deg_max - const_deg[o];
                }
                for (size_t r = 0; r < op->q; r++) {
                    if (r == s)
                        IX_S(ix, op, k, r) = var_deg_max[k] - var_deg[k][o];
                    else
                        IX_S(ix, op, k, r) = var_deg_max[k];
                }
                IX_Z(ix, op, k) = 1;
                IX_W(ix, op, k) = 1;
                mpz_set(inps[0], delta[k][s][o]);
                mpz_set(inps[1], gamma[k][s][o]);
                sprintf(tmp, "zhat[%lu,%lu,%lu]", k, s, o);
                my_encoding_print(tmp, inps, ix);
                __encode(pool, obf->enc_vt, obf->zhat[k][s][o], inps, obf_index_copy(ix, op), obf->sp);

                obf_index_clear(ix);
                IX_W(ix, op, k) = 1;
                mpz_set_ui(inps[0], 0);
                mpz_set   (inps[1], gamma[k][s][o]);
                sprintf(tmp, "what[%lu,%lu,%lu]", k, s, o);
                my_encoding_print(tmp, inps, ix);
                __encode(pool, obf->enc_vt, obf->what[k][s][o], inps, obf_index_copy(ix, op), obf->sp);
                {
                    count += 2;
                    print_progress(count, total);
                }
            }
        }
    }

    for (size_t i = 0; i < op->m; i++) {
        obf_index_clear(ix);
        IX_Y(ix) = 1;
        mpz_set_ui(inps[0], c->consts[i]);
        mpz_set   (inps[1], beta[i]);
        sprintf(tmp, "yhat[%lu]", i);
        my_encoding_print(tmp, inps, ix);
        __encode(pool, obf->enc_vt, obf->yhat[i], inps, obf_index_copy(ix, op), obf->sp);
        {
            print_progress(++count, total);
        }
    }
    for (size_t p = 0; p < op->npowers; p++) {
        obf_index_clear(ix);
        IX_Y(ix) = 1 << p;
        mpz_set_ui(inps[0], 1);
        mpz_set_ui(inps[1], 1);
        sprintf(tmp, "vhat[%lu]", p);
        my_encoding_print(tmp, inps, ix);
        __encode(pool, obf->enc_vt, obf->vhat[p], inps, obf_index_copy(ix, op), obf->sp);
        {
            print_progress(++count, total);
        }
    }

    for (size_t i = 0; i < op->gamma; i++) {
        obf_index_clear(ix);
        IX_Y(ix) = const_deg_max;
        for (size_t k = 0; k < op->c; k++) {
            for (size_t j = 0; j < op->q; j++) {
                IX_S(ix, op, k, j) = var_deg_max[k];
            }
            IX_Z(ix, op, k) = 1;
        }

        mpz_set_ui(inps[0], 0);
        mpz_set   (inps[1], Cstar[i]);

        sprintf(tmp, "Chatstar[%lu]", i);
        my_encoding_print(tmp, inps, ix);
        __encode(pool, obf->enc_vt, obf->Chatstar[i], inps, obf_index_copy(ix, op), obf->sp);
        {
            print_progress(++count, total);
        }
    }

    threadpool_destroy(pool);

    obf_index_free(ix);
    mpz_vect_clear(inps, 2);

    for (size_t k = 0; k < op->c; k++) {
        for (size_t j = 0; j < op->ell; j++) {
            mpz_clear(alpha[k][j]);
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
    for (size_t k = 0; k < op->gamma; k++) {
        encoding_fwrite(obf->enc_vt, obf->Chatstar[k], fp);
    }
    return OK;
}

static obfuscation *
_obfuscator_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    obfuscation *const obf = my_calloc(1, sizeof(obfuscation));
    if (obf == NULL)
        return NULL;
    obf->mmap = mmap;
    obf->pp_vt = get_pp_vtable(mmap);
    obf->sp_vt = get_sp_vtable(mmap);
    obf->enc_vt = get_encoding_vtable(mmap);
    obf->op = op;
    obf->sp = NULL;
    obf->pp = public_params_fread(obf->pp_vt, op, fp);

    obf->shat = my_calloc(op->c, sizeof(encoding ***));
    obf->uhat = my_calloc(op->c, sizeof(encoding ***));
    obf->zhat = my_calloc(op->c, sizeof(encoding ***));
    obf->what = my_calloc(op->c, sizeof(encoding ***));
    for (size_t k = 0; k < op->c; k++) {
        obf->shat[k] = my_calloc(op->q, sizeof(encoding **));
        obf->uhat[k] = my_calloc(op->q, sizeof(encoding **));
        obf->zhat[k] = my_calloc(op->q, sizeof(encoding **));
        obf->what[k] = my_calloc(op->q, sizeof(encoding **));
        for (size_t s = 0; s < op->q; s++) {
            obf->shat[k][s] = my_calloc(op->ell, sizeof(encoding *));
            for (size_t j = 0; j < op->ell; j++) {
                obf->shat[k][s][j] = encoding_fread(obf->enc_vt, fp);
            }
            obf->uhat[k][s] = my_calloc(op->npowers, sizeof(encoding *));
            for (size_t p = 0; p < op->npowers; p++) {
                obf->uhat[k][s][p] = encoding_fread(obf->enc_vt, fp);
            }
            obf->zhat[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            obf->what[k][s] = my_calloc(op->gamma, sizeof(encoding *));
            for (size_t o = 0; o < op->gamma; o++) {
                obf->zhat[k][s][o] = encoding_fread(obf->enc_vt, fp);
                obf->what[k][s][o] = encoding_fread(obf->enc_vt, fp);
            }
        }
    }
    obf->yhat = my_calloc(op->m, sizeof(encoding *));
    for (size_t i = 0; i < op->m; i++) {
        obf->yhat[i] = encoding_fread(obf->enc_vt, fp);
    }
    obf->vhat = my_calloc(op->npowers, sizeof(encoding *));
    for (size_t p = 0; p < op->npowers; p++) {
        obf->vhat[p] = encoding_fread(obf->enc_vt, fp);
    }
    obf->Chatstar = my_calloc(op->gamma, sizeof(encoding *));
    for (size_t i = 0; i < op->gamma; i++) {
        obf->Chatstar[i] = encoding_fread(obf->enc_vt, fp);
    }
    return obf;
}

/*******************************************************************************/

typedef struct ref_list_node {
    acircref ref;
    struct ref_list_node *next;
} ref_list_node;

typedef struct {
    ref_list_node *first;
    pthread_mutex_t *lock;
} ref_list;

typedef struct work_args {
    const mmap_vtable *mmap;
    acircref ref;
    const acirc *c;
    const int *inputs;
    const obfuscation *obf;
    int *mine;
    int *ready;
    encoding **cache;
    ref_list **deps;
    threadpool *pool;
    int *rop;
} work_args;

static void obf_eval_worker(void *wargs);

static ref_list *ref_list_create(void);
static void ref_list_destroy(ref_list *list);
static void ref_list_push(ref_list *list, acircref ref);

static void raise_encoding(const obfuscation *obf, encoding *x, const obf_index *target)
{
    obf_index *const ix =
        obf_index_difference(obf->op, target, obf->enc_vt->mmap_set(x));
    for (size_t k = 0; k < obf->op->c; k++) {
        for (size_t s = 0; s < obf->op->q; s++) {
            size_t diff = IX_S(ix, obf->op, k, s);
            while (diff > 0) {
                // want to find the largest power we obfuscated to multiply by
                size_t p = 0;
                while (((size_t) (1 << (p+1)) <= diff) && ((p+1) < obf->op->npowers))
                    p++;
                encoding_mul(obf->enc_vt, obf->pp_vt, x, x, obf->uhat[k][s][p], obf->pp);
                diff -= (1 << p);
            }
        }
    }
    size_t diff = IX_Y(ix);
    while (diff > 0) {
        size_t p = 0;
        while (((size_t) (1 << (p+1)) <= diff) && ((p+1) < obf->op->npowers))
            p++;
        encoding_mul(obf->enc_vt, obf->pp_vt, x, x, obf->vhat[p], obf->pp);
        diff -= (1 << p);
    }
    obf_index_free(ix);
}

static void
raise_encodings(const obfuscation *obf, encoding *x, encoding *y)
{
    if (obf_index_eq(obf->enc_vt->mmap_set(x), obf->enc_vt->mmap_set(y)))
        return;

    obf_index *const ix = obf_index_union(obf->op, obf->enc_vt->mmap_set(x),
                                          obf->enc_vt->mmap_set(y));
    raise_encoding(obf, x, ix);
    raise_encoding(obf, y, ix);
    obf_index_free(ix);
}

static int
_evaluate(int *rop, const int *inputs, const obfuscation *obf, size_t nthreads)
{
    const acirc *const c = obf->op->circ;

    if (LOG_DEBUG) {
        fprintf(stderr, "[%s] evaluating on input ", __func__);
        for (size_t i = 0; i < obf->op->c; ++i)
            fprintf(stderr, "%d", inputs[obf->op->c - 1 - i]);
        fprintf(stderr, "\n");
    }

    encoding **cache;
    ref_list **deps;
    int *mine, *ready;

    // evaluated intermediate nodes
    cache = my_calloc(c->nrefs, sizeof(encoding *));
    // each list contains refs of nodes dependent on this one
    deps = my_calloc(c->nrefs, sizeof(ref_list));
    // whether the evaluator allocated an encoding in cache
    mine = my_calloc(c->nrefs, sizeof(int));
    // number of children who have been evaluated already
    ready = my_calloc(c->nrefs, sizeof(int));

    for (size_t i = 0; i < c->nrefs; i++) {
        cache[i] = NULL;
        deps [i] = ref_list_create();
        mine [i] = 0;
        ready[i] = 0;
    }

    /* Make sure inputs are valid */
    int input_syms[obf->op->c];
    for (size_t i = 0; i < obf->op->c; i++) {
        input_syms[i] = 0;
        for (size_t j = 0; j < obf->op->ell; j++) {
            const sym_id sym = { i, j };
            const acircref k = obf->op->rchunker(sym, c->ninputs, obf->op->c);
            if (obf->op->rachel_inputs)
                input_syms[i] += inputs[k] * j;
            else
                input_syms[i] += inputs[k] << j;
        }
        if (input_syms[i] >= obf->op->q) {
            fprintf(stderr, "error: invalid input (%d > |Σ|)\n", input_syms[i]);
            return ERR;
        }
    }

    // populate dependents lists
    for (size_t ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->gates[ref].op;
        if (op == OP_INPUT || op == OP_CONST)
            continue;
        acircref x = c->gates[ref].args[0];
        acircref y = c->gates[ref].args[1];
        ref_list_push(deps[x], ref);
        ref_list_push(deps[y], ref);
    }

    threadpool *pool = threadpool_create(nthreads);

    // start threads evaluating the circuit inputs- they will signal their
    // parents to start, recursively, until the output is reached.
    for (size_t ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->gates[ref].op;
        if (!(op == OP_INPUT || op == OP_CONST)) {
            continue;
        }
        // allocate each argstruct here, otherwise we will overwrite
        // it each time we add to the job list. The worker will free.
        work_args *args = calloc(1, sizeof(work_args));
        args->mmap   = obf->mmap;
        args->ref    = ref;
        args->c      = c;
        args->inputs = inputs;
        args->obf    = obf;
        args->mine   = mine;
        args->ready  = ready;
        args->cache  = cache;
        args->deps   = deps;
        args->pool   = pool;
        args->rop    = rop;
        threadpool_add_job(pool, obf_eval_worker, args);
    }

    // threadpool_destroy waits for all the jobs to finish
    threadpool_destroy(pool);

    // cleanup
    for (size_t i = 0; i < c->nrefs; i++) {
        ref_list_destroy(deps[i]);
        if (mine[i]) {
            encoding_free(obf->enc_vt, cache[i]);
        }
    }

    free(cache);
    free(deps);
    free(mine);
    free(ready);

    if (LOG_DEBUG) {
        fprintf(stderr, "[%s] result: ", __func__);
        for (size_t i = 0; i < obf->op->gamma; ++i)
            fprintf(stderr, "%d", rop[obf->op->gamma - 1 - i]);
        fprintf(stderr, "\n");
    }

    return OK;
}

void obf_eval_worker(void* wargs)
{
    const acircref ref = ((work_args*)wargs)->ref;
    const acirc *const c = ((work_args*)wargs)->c;
    const int *const inputs = ((work_args*)wargs)->inputs;
    const obfuscation *const obf = ((work_args*)wargs)->obf;
    int *const mine  = ((work_args*)wargs)->mine;
    int *const ready = ((work_args*)wargs)->ready;
    encoding **cache = ((work_args*)wargs)->cache;
    ref_list **deps  = ((work_args*)wargs)->deps;
    threadpool *pool = ((work_args*)wargs)->pool;
    int *const rop   = ((work_args*)wargs)->rop;

    const acirc_operation op = c->gates[ref].op;
    const acircref *const args = c->gates[ref].args;
    encoding *res;

    int input_syms[obf->op->c];
    for (size_t i = 0; i < obf->op->c; i++) {
        input_syms[i] = 0;
        for (size_t j = 0; j < obf->op->ell; j++) {
            const sym_id sym = { i, j };
            const acircref k = obf->op->rchunker(sym, c->ninputs, obf->op->c);
            if (obf->op->rachel_inputs)
                input_syms[i] += inputs[k] * j;
            else
                input_syms[i] += inputs[k] << j;
        }
    }

    switch (op) {
    case OP_INPUT: {
        const size_t id = args[0];
        const sym_id sym = obf->op->chunker(id, c->ninputs, obf->op->c);
        const size_t k = sym.sym_number;
        const size_t s = input_syms[k];
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
        res = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        mine[ref] = true;

        // the encodings of the args exist since the ref's children signalled it
        const encoding *const x = cache[args[0]];
        const encoding *const y = cache[args[1]];

        if (op == OP_MUL) {
            encoding_mul(obf->enc_vt, obf->pp_vt, res, x, y, obf->pp);
        } else {
            encoding *tmp_x, *tmp_y;

            tmp_x = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            tmp_y = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            encoding_set(obf->enc_vt, tmp_x, x);
            encoding_set(obf->enc_vt, tmp_y, y);
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
    default:
        abort();
    }        

    // set the result in the cache
    cache[ref] = res;

    // signal parents that this ref is done
    ref_list_node *cur = deps[ref]->first;
    while (cur != NULL) {
        pthread_mutex_lock(deps[cur->ref]->lock);
        ready[cur->ref] += 1; // ready[ref] indicates how many of ref's children are evaluated
        if (ready[cur->ref] == 2) {
            pthread_mutex_unlock(deps[cur->ref]->lock);
            work_args *newargs = calloc(1, sizeof(work_args));
            *newargs = *(work_args*)wargs;
            newargs->ref = cur->ref;
            threadpool_add_job(pool, obf_eval_worker, (void*)newargs);
        } else {
            pthread_mutex_unlock(deps[cur->ref]->lock);
            cur = cur->next;
        }
    }
    free(wargs);

    // addendum: is this ref an output bit? if so, we should zero test it.
    size_t output;
    bool is_output = false;
    for (size_t i = 0; i < obf->op->gamma; i++) {
        if (ref == c->outrefs[i]) {
            output = i;
            is_output = true;
            break;
        }
    }

    if (is_output) {
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
                         obf->what[k][input_syms[k]][output], obf->pp);
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
                         obf->zhat[k][input_syms[k]][output], obf->pp);
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
        rop[output] = !encoding_is_zero(obf->enc_vt, obf->pp_vt, out, obf->pp);

    cleanup:
        encoding_free(obf->enc_vt, out);
        encoding_free(obf->enc_vt, lhs);
        encoding_free(obf->enc_vt, rhs);
        encoding_free(obf->enc_vt, tmp);
        encoding_free(obf->enc_vt, tmp2);
    }
}


////////////////////////////////////////////////////////////////////////////////
// ref list utils

static ref_list *
ref_list_create(void)
{
    ref_list *list = calloc(1, sizeof(ref_list));
    list->first = NULL;
    list->lock = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(list->lock, NULL);
    return list;
}

static void
ref_list_destroy(ref_list *list)
{
    ref_list_node *cur = list->first;
    while (cur != NULL) {
        ref_list_node *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    pthread_mutex_destroy(list->lock);
    free(list->lock);
    free(list);
}

static ref_list_node *
ref_list_node_create(acircref ref)
{
    ref_list_node *new = calloc(1, sizeof(ref_list_node));
    new->next = NULL;
    new->ref  = ref;
    return new;
}

static void
ref_list_push(ref_list *list, acircref ref)
{
    ref_list_node *cur = list->first;
    if (cur == NULL) {
        list->first = ref_list_node_create(ref);
        return;
    }
    while (1) {
        if (cur->next == NULL) {
            cur->next = ref_list_node_create(ref);
            return;
        }
        cur = cur->next;
    }
}

obfuscator_vtable lz_obfuscator_vtable = {
    .new =  _obfuscator_new,
    .free = _obfuscator_free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _obfuscator_fwrite,
    .fread = _obfuscator_fread,
};
