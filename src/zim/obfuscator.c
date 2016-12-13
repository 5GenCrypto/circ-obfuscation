#include "obfuscator.h"
#include "obf_index.h"
#include "obf_params.h"
#include "vtables.h"

#include <assert.h>
#include <threadpool.h>
#include <pthread.h>

typedef struct obfuscation {
    const mmap_vtable *mmap;
    const pp_vtable *pp_vt;
    const sp_vtable *sp_vt;
    const encoding_vtable *enc_vt;
    const obf_params_t *op;
    secret_params *sp;
    public_params *pp;
    aes_randstate_t rng;
    encoding ***xhat;       // [n][2]
    encoding ****uhat;      // [n][2][npowers]
    encoding ****zhat;      // [n][2][o]
    encoding ****what;      // [n][2][o]
    encoding **yhat;        // [m]
    encoding **vhat;        // [npowers]
    encoding **Chatstar;    // [o]
} obfuscation;

#define NUM_ENCODINGS(C, NPOWERS) ( \
        (C)->ninputs * 2 + \
        (C)->ninputs * 2 * (NPOWERS) + \
        (C)->ninputs * 2 * (C)->noutputs * 2 + \
        (C)->nconsts + \
        (NPOWERS) + \
        (C)->noutputs \
    )

static void
_obfuscator_free(obfuscation *obf);

static obfuscation *
_obfuscator_new(const mmap_vtable *mmap, const obf_params_t *const op,
                size_t secparam)
{
    obfuscation *obf;

    obf = calloc(1, sizeof(obfuscation));
    obf->mmap = mmap;
    obf->enc_vt = zim_get_encoding_vtable(mmap);
    obf->pp_vt = zim_get_pp_vtable(mmap);
    obf->sp_vt = zim_get_sp_vtable(mmap);
    obf->op = op;
    aes_randinit(obf->rng);
    obf->sp = calloc(1, sizeof(secret_params));
    if (secret_params_init(obf->sp_vt, obf->sp, op, secparam, obf->rng)) {
        _obfuscator_free(obf);
        return NULL;
    }
    obf->pp = calloc(1, sizeof(public_params));
    public_params_init(obf->pp_vt, obf->sp_vt, obf->pp, obf->sp);

    obf->xhat = calloc(op->ninputs, sizeof(encoding **));
    obf->uhat = calloc(op->ninputs, sizeof(encoding ***));
    obf->zhat = calloc(op->ninputs, sizeof(encoding ***));
    obf->what = calloc(op->ninputs, sizeof(encoding ***));
    for (size_t i = 0; i < op->ninputs; i++) {
        obf->xhat[i] = calloc(2, sizeof(encoding *));
        obf->uhat[i] = calloc(2, sizeof(encoding **));
        obf->zhat[i] = calloc(2, sizeof(encoding **));
        obf->what[i] = calloc(2, sizeof(encoding **));
        for (size_t b = 0; b < 2; b++) {
            obf->xhat[i][b] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            obf->uhat[i][b] = calloc(op->npowers, sizeof(encoding *));
            obf->zhat[i][b] = calloc(op->noutputs, sizeof(encoding *));
            obf->what[i][b] = calloc(op->noutputs, sizeof(encoding *));
            for (size_t p = 0; p < op->npowers; p++) {
                obf->uhat[i][b][p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
            for (size_t k = 0; k < op->noutputs; k++) {
                obf->zhat[i][b][k] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
                obf->what[i][b][k] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            }
        }
    }
    obf->yhat = calloc(op->nconsts, sizeof(encoding *));
    for (size_t j = 0; j < op->nconsts; j++) {
        obf->yhat[j] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    obf->vhat = calloc(op->npowers, sizeof(encoding *));
    for (size_t p = 0; p < op->npowers; p++) {
        obf->vhat[p] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    }
    obf->Chatstar = calloc(op->noutputs, sizeof(encoding *));
    for (size_t i = 0; i < op->noutputs; i++) {
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

    for (size_t i = 0; i < op->ninputs; i++) {
        for (size_t b = 0; b <= 1; b++) {
            encoding_free(obf->enc_vt, obf->xhat[i][b]);
            for (size_t p = 0; p < op->npowers; p++) {
                encoding_free(obf->enc_vt, obf->uhat[i][b][p]);
            }
            free(obf->uhat[i][b]);
            for (size_t k = 0; k < op->noutputs; k++) {
                encoding_free(obf->enc_vt, obf->zhat[i][b][k]);
                encoding_free(obf->enc_vt, obf->what[i][b][k]);
            }
            free(obf->zhat[i][b]);
            free(obf->what[i][b]);
        }
        free(obf->xhat[i]);
        free(obf->uhat[i]);
        free(obf->zhat[i]);
        free(obf->what[i]);
    }
    free(obf->xhat);
    free(obf->uhat);
    free(obf->zhat);
    free(obf->what);
    for (size_t j = 0; j < op->nconsts; j++) {
        encoding_free(obf->enc_vt, obf->yhat[j]);
    }
    free(obf->yhat);
    for (size_t p = 0; p < op->npowers; p++) {
        encoding_free(obf->enc_vt, obf->vhat[p]);
    }
    free(obf->vhat);
    for (size_t i = 0; i < op->noutputs; i++) {
        encoding_free(obf->enc_vt, obf->Chatstar[i]);
    }
    free(obf->Chatstar);

    if (obf->pp) {
        public_params_clear(obf->pp_vt, obf->pp);
        free(obf->pp);
    }
    if (obf->sp) {
        secret_params_clear(obf->sp_vt, obf->sp);
        free(obf->sp);
    }

    free(obf);
}



static void
_obfuscate(obfuscation *const obf)
{
    const obf_params_t *op = obf->op;
    const acirc *c = op->circ;
    const mpz_t *moduli =
        mpz_vect_create_of_fmpz(obf->mmap->sk->plaintext_fields(obf->sp->sk),
                                obf->mmap->sk->nslots(obf->sp->sk));
    const size_t n = op->ninputs;
    const size_t m = op->nconsts;
    const size_t o = op->noutputs;

    mpz_t ones[2];
    mpz_t inps[2];
    mpz_t alpha[n];
    mpz_t gamma[n][2][o];
    mpz_t delta[n][2][o];

    size_t encode_ct = 0;
    size_t encode_n  = NUM_ENCODINGS(op->circ, op->npowers);
    print_progress(encode_ct, encode_n);

    mpz_vect_init(ones, 2);
    mpz_vect_init(inps, 2);
    mpz_set_ui(ones[0], 1);
    mpz_set_ui(ones[1], 1);

    assert(obf->mmap->sk->nslots(obf->sp->sk) >= 2);

/* #pragma omp parallel for */
    for (size_t i = 0; i < op->ninputs; i++) {
        mpz_init(alpha[i]);
        mpz_randomm_inv(alpha[i], obf->rng, moduli[1]);
        for (size_t b = 0; b <= 1; b++) {
            for (size_t k = 0; k < op->noutputs; k++) {
                mpz_inits(gamma[i][b][k], delta[i][b][k], NULL);
                mpz_randomm_inv(gamma[i][b][k], obf->rng, moduli[1]);
                mpz_randomm_inv(delta[i][b][k], obf->rng, moduli[0]);
            }
        }
    }

    mpz_t beta[m];
/* #pragma omp parallel for */
    for (size_t j = 0; j < op->nconsts; j++) {
        mpz_init(beta[j]);
        mpz_randomm_inv(beta[j], obf->rng, moduli[1]);
    }

    unsigned long con_deg[o];
    unsigned long con_dmax;
    unsigned long var_deg[n][o];
    unsigned long var_dmax[n];
/* #pragma omp parallel for */
    for (size_t k = 0; k < op->noutputs; k++) {
        con_deg[k] = acirc_const_degree(c, c->outrefs[k]);
    }
    for (size_t k = 0; k < op->noutputs; k++) {
        if (k == 0 || con_deg[k] > con_dmax)
            con_dmax = con_deg[k];
    }
/* #pragma omp parallel for */
    for (size_t i = 0; i < op->noutputs; i++) {
        var_dmax[i] = 0;
    }
/* #pragma omp parallel for schedule(dynamic,1) collapse(2) */
    for (size_t i = 0; i < op->ninputs; i++) {
        for (size_t k = 0; k < op->noutputs; k++) {
            var_deg[i][k] = acirc_var_degree(c, c->outrefs[k], i);
        }
    }
    for (size_t i = 0; i < op->ninputs; i++) {
        for (size_t k = 0; k < op->noutputs; k++) {
            if (i == 0 || var_deg[i][k] > var_dmax[i])
                var_dmax[i] = var_deg[i][k];
        }
    }

/* #pragma omp parallel for */
    for (size_t i = 0; i < op->ninputs; i++) {
        for (size_t b = 0; b <= 1; b++) {
            obf_index *ix_x;

            mpz_set_ui(inps[0], b);
            mpz_set   (inps[1], alpha[i]);

            // create the xhat and uhat encodings
            ix_x = obf_index_create(n);
            IX_X(ix_x, i, b) = 1;
            encode(obf->enc_vt, obf->xhat[i][b], inps, 2, ix_x, obf->sp);
            for (size_t p = 0; p < op->npowers; p++) {
                IX_X(ix_x, i, b) = 1 << p;
                encode(obf->enc_vt, obf->uhat[i][b][p], ones, 2, ix_x, obf->sp);
            }
            obf_index_destroy(ix_x);

/* #pragma omp critical */
            {
                encode_ct += 1 + op->npowers;
                print_progress(encode_ct, encode_n);
            }

            for (size_t k = 0; k < op->noutputs; k++) {
                // create the zhat encodings for each output wire
                {
                    obf_index *ix = obf_index_create(n);
                    if (i == 0) {
                        IX_Y(ix) = con_dmax - con_deg[k];
                    }
                    IX_X(ix, i, b)   = var_dmax[i] - var_deg[i][k];
                    IX_X(ix, i, 1-b) = var_dmax[i];
                    IX_Z(ix, i) = 1;
                    IX_W(ix, i) = 1;

                    mpz_set(inps[0], delta[i][b][k]);
                    mpz_set(inps[1], gamma[i][b][k]);
                    encode(obf->enc_vt, obf->zhat[i][b][k], inps, 2, ix, obf->sp);
                    obf_index_destroy(ix);
                }
                // create the what encodings
                {
                    obf_index *ix = obf_index_create(n);
                    IX_W(ix, i) = 1;
                    mpz_set_ui(inps[0], 0);
                    mpz_set   (inps[1], gamma[i][b][k]);
                    encode(obf->enc_vt, obf->what[i][b][k], inps, 2, ix, obf->sp);
                    obf_index_destroy(ix);
                }

/* #pragma omp critical */
                {
                    encode_ct += 2;
                    print_progress(encode_ct, encode_n);
                }
            }
        }
    }

    // create the yhat and vhat encodings
    obf_index *ix_y = obf_index_create(n);
    IX_Y(ix_y) = 1;
/* #pragma omp parallel for */
    for (int j = 0; j < op->nconsts; j++) {
        mpz_set_ui(inps[0], c->consts[j]);
        mpz_set   (inps[1], beta[j]);
        encode(obf->enc_vt, obf->yhat[j], inps, 2, ix_y, obf->sp);
/* #pragma omp critical */
        {
            print_progress(++encode_ct, encode_n);
        }
    }
    for (size_t p = 0; p < op->npowers; p++) {
        IX_Y(ix_y) = 1 << p;
        encode(obf->enc_vt, obf->vhat[p], ones, 2, ix_y, obf->sp);
        print_progress(++encode_ct, encode_n);
    }
    obf_index_destroy(ix_y);

    // use memoized circuit evaluation instead of re-eval each time!
    bool  known[c->nrefs];
    mpz_t cache[c->nrefs];
    for (size_t i = 0; i < c->nrefs; i++)
        known[i] = false;
    mpz_t Cstar[o];
    for (size_t k = 0; k < o; k++) {
        mpz_init(Cstar[k]);
        acirc_eval_mpz_mod_memo(Cstar[k], c, c->outrefs[k], alpha, beta, moduli[1], known, cache);
    }
    for (size_t i = 0; i < c->nrefs; i++) {
        if (known[i])
            mpz_clear(cache[i]);
    }

/* #pragma omp parallel for */
    for (size_t k = 0; k < o; k++) {
        obf_index *ix_c = obf_index_create(n);
        IX_Y(ix_c) = con_dmax; // acirc_max_const_degree(c);

        for (size_t i = 0; i < n; i++) {
            unsigned long d = var_dmax[i]; // acirc_max_var_degree(c, i);
            IX_X(ix_c, i, 0) = d;
            IX_X(ix_c, i, 1) = d;
            IX_Z(ix_c, i) = 1;
        }

        encode(obf->enc_vt, obf->Chatstar[k], &Cstar[k], 1, ix_c, obf->sp);
        obf_index_destroy(ix_c);

/* #pragma omp critical */
        {
            print_progress(++encode_ct, encode_n);
        }
    }
    puts("");

    // cleanup
    mpz_vect_clear(ones, 2);
    mpz_vect_clear(inps, 2);

/* #pragma omp parallel for */
    for (size_t i = 0; i < n; i++) {
        mpz_clear(alpha[i]);
        for (size_t b = 0; b <= 1; b++) {
            for (size_t k = 0; k < o; k++) {
                mpz_clears(delta[i][b][k], gamma[i][b][k], NULL);
            }
        }
    }
/* #pragma omp parallel for */
    for (size_t j = 0; j < m; j++)
        mpz_clear(beta[j]);
/* #pragma omp parallel for */
    for (size_t k = 0; k < o; k++)
        mpz_clear(Cstar[k]);
    /* free(moduli); */
}

static void
_obfuscator_fwrite(const obfuscation *const obf, FILE *const fp)
{
    const obf_params_t *const op = obf->op;
    /* ulong_fwrite(op->ninputs, fp); */
    /* ulong_fwrite(op->nconsts, fp); */
    /* ulong_fwrite(op->noutputs, fp); */
    /* ulong_fwrite(op->npowers, fp); */
    public_params_fwrite(obf->pp_vt, obf->pp, fp);
    for (size_t i = 0; i < op->ninputs; i++) {
        for (size_t b = 0; b <= 1; b++) {
            encoding_fwrite(obf->enc_vt, obf->xhat[i][b], fp);
            for (size_t p = 0; p < op->npowers; p++) {
                encoding_fwrite(obf->enc_vt, obf->uhat[i][b][p], fp);
            }
            for (size_t k = 0; k < op->noutputs; k++) {
                encoding_fwrite(obf->enc_vt, obf->zhat[i][b][k], fp);
                encoding_fwrite(obf->enc_vt, obf->what[i][b][k], fp);
            }
        }
    }
    for (size_t j = 0; j < op->nconsts; j++) {
        encoding_fwrite(obf->enc_vt, obf->yhat[j], fp);
    }
    for (size_t p = 0; p < op->npowers; p++) {
        encoding_fwrite(obf->enc_vt, obf->vhat[p], fp);
    }
    for (size_t k = 0; k < op->noutputs; k++) {
        encoding_fwrite(obf->enc_vt, obf->Chatstar[k], fp);
    }
}

static obfuscation *
_obfuscator_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *const fp)
{
    obfuscation *obf;

    obf = calloc(1, sizeof(obfuscation));
    if (obf == NULL)
        return NULL;
    obf->mmap = mmap;
    obf->pp_vt = zim_get_pp_vtable(mmap);
    obf->sp_vt = zim_get_sp_vtable(mmap);
    obf->enc_vt = zim_get_encoding_vtable(mmap);
    obf->op = op;
    obf->sp = NULL;
    /* ulong_fread(&op->ninputs, fp); */
    /* ulong_fread(&op->nconsts, fp); */
    /* ulong_fread(&op->noutputs, fp); */
    /* ulong_fread(&op->npowers, fp); */
    obf->pp = calloc(1, sizeof(public_params));
    public_params_fread(obf->pp_vt, obf->pp, op, fp);

    obf->xhat = calloc(op->ninputs, sizeof(encoding **));
    obf->uhat = calloc(op->ninputs, sizeof(encoding ***));
    obf->zhat = calloc(op->ninputs, sizeof(encoding ***));
    obf->what = calloc(op->ninputs, sizeof(encoding ***));
    for (size_t i = 0; i < op->ninputs; i++) {
        obf->xhat[i] = calloc(2, sizeof(encoding *));
        obf->uhat[i] = calloc(2, sizeof(encoding **));
        obf->zhat[i] = calloc(2, sizeof(encoding **));
        obf->what[i] = calloc(2, sizeof(encoding **));
        for (size_t b = 0; b <= 1; b++) {
            obf->xhat[i][b] = calloc(1, sizeof(encoding));
            encoding_fread(obf->enc_vt, obf->xhat[i][b], fp);
            obf->uhat[i][b] = calloc(op->npowers, sizeof(encoding *));
            for (size_t p = 0; p < op->npowers; p++) {
                obf->uhat[i][b][p] = calloc(1, sizeof(encoding));
                encoding_fread(obf->enc_vt, obf->uhat[i][b][p], fp);
            }
            obf->zhat[i][b] = calloc(op->noutputs, sizeof(encoding *));
            obf->what[i][b] = calloc(op->noutputs, sizeof(encoding *));
            for (size_t k = 0; k < op->noutputs; k++) {
                obf->zhat[i][b][k] = calloc(1, sizeof(encoding));
                encoding_fread(obf->enc_vt, obf->zhat[i][b][k], fp);
                obf->what[i][b][k] = calloc(1, sizeof(encoding));
                encoding_fread(obf->enc_vt, obf->what[i][b][k], fp);
            }
        }
    }
    obf->yhat = calloc(op->nconsts, sizeof(encoding *));
    for (size_t j = 0; j < op->nconsts; j++) {
        obf->yhat[j] = calloc(1, sizeof(encoding));
        encoding_fread(obf->enc_vt, obf->yhat[j], fp);
    }
    obf->vhat = calloc(op->npowers, sizeof(encoding *));
    for (size_t j = 0; j < op->npowers; j++) {
        obf->vhat[j] = calloc(1, sizeof(encoding));
        encoding_fread(obf->enc_vt, obf->vhat[j], fp);
    }
    obf->Chatstar = calloc(op->noutputs, sizeof(encoding *));
    for (size_t k = 0; k < op->noutputs; k++) {
        obf->Chatstar[k] = calloc(1, sizeof(encoding));
        encoding_fread(obf->enc_vt, obf->Chatstar[k], fp);
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
static void raise_encodings(const obfuscation *const obf, encoding *const x,
                            encoding *const y);
static void raise_encoding(const obfuscation *const obf, encoding *const x,
                           obf_index *const target);

static void
_evaluate(int *rop, const int *inputs, const obfuscation *const obf)
{
    const acirc *const c = obf->op->circ;

    encoding *cache[c->nrefs];  // evaluated intermediate nodes
    ref_list *deps[c->nrefs]; // each list contains refs of nodes dependent on this one
    int mine[c->nrefs];  // whether the evaluator allocated an encoding in cache
    int ready[c->nrefs];   // number of children who have been evaluated already

    for (size_t i = 0; i < c->nrefs; i++) {
        cache[i] = NULL;
        deps [i] = ref_list_create();
        mine [i] = 0;
        ready[i] = 0;
    }

    // populate dependents lists
    for (acircref ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->gates[ref].op;
        if (op == XINPUT || op == YINPUT)
            continue;
        acircref x = c->gates[ref].args[0];
        acircref y = c->gates[ref].args[1];
        ref_list_push(deps[x], ref);
        ref_list_push(deps[y], ref);
    }

    threadpool *pool = threadpool_create(NCORES);

    // start threads evaluating the circuit inputs- they will signal their
    // parents to start, recursively, until the output is reached.
    for (acircref ref = 0; ref < c->nrefs; ref++) {
        acirc_operation op = c->gates[ref].op;
        if (!(op == XINPUT || op == YINPUT)) {
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
}

void obf_eval_worker(void* wargs)
{
    /* const mmap_vtable *const mmap = ((work_args*)wargs)->mmap; */
    acircref ref     = ((work_args*)wargs)->ref; // the particular ref to evaluate right now
    const acirc *const c = ((work_args*)wargs)->c;
    const int *const inputs = ((work_args*)wargs)->inputs;
    const obfuscation *const obf = ((work_args*)wargs)->obf;
    int *mine        = ((work_args*)wargs)->mine;
    int *ready       = ((work_args*)wargs)->ready;
    encoding **cache = ((work_args*)wargs)->cache;
    ref_list **deps  = ((work_args*)wargs)->deps;
    threadpool *pool = ((work_args*)wargs)->pool;
    int *rop         = ((work_args*)wargs)->rop;

    acirc_operation op = c->gates[ref].op;
    acircref *args     = c->gates[ref].args;
    encoding *res;

    // if the ref is input or const, return the approprite encoding from the obfuscation
    if (op == XINPUT) {
        size_t xid = args[0];
        res = obf->xhat[xid][inputs[xid]];
        mine[ref] = 0; // the evaluator didn't allocate this encoding
    }
    else if (op == YINPUT) {
        size_t yid = args[0];
        res = obf->yhat[yid];
        mine[ref] = 0;
    }

    // otherwise the ref is some kind of gate: allocate the encoding & eval
    else {
        res = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        mine[ref] = 1; // the evaluator allocated this encoding

        // the encodings of the args exist since the ref's children signalled it
        encoding *x = cache[args[0]];
        encoding *y = cache[args[1]];
        assert(x != NULL);
        assert(y != NULL);

        if (op == MUL) {
            encoding_mul(obf->enc_vt, obf->pp_vt, res, x, y, obf->pp);
        }
        else {
            encoding *tmp_x, *tmp_y;

            tmp_x = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            tmp_y = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
            encoding_set(obf->enc_vt, tmp_x, x);
            encoding_set(obf->enc_vt, tmp_y, y);
            raise_encodings(obf, tmp_x, tmp_y);
            if (op == ADD) {
                encoding_add(obf->enc_vt, obf->pp_vt, res, tmp_x, tmp_y, obf->pp);
            } else if (op == SUB) {
                encoding_sub(obf->enc_vt, obf->pp_vt, res, tmp_x, tmp_y, obf->pp);
            }
            encoding_free(obf->enc_vt, tmp_x);
            encoding_free(obf->enc_vt, tmp_y);
        }
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
    free((work_args*)wargs);

    // addendum: is this ref an output bit? if so, we should zero test it.
    int k;
    int ref_is_output = 0;
    for (size_t i = 0; i < c->noutputs; i++) {
        if (ref == c->outrefs[i]) {
            k = i;
            ref_is_output = 1;
            break;
        }
    }

    if (ref_is_output) {
        encoding *outwire, *tmp;

        outwire = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        tmp = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
        
        encoding_set(obf->enc_vt, outwire, res);
        encoding_set(obf->enc_vt, tmp, obf->Chatstar[k]);

        for (size_t i = 0; i < c->ninputs; i++)
            encoding_mul(obf->enc_vt, obf->pp_vt, outwire, outwire, obf->zhat[i][inputs[i]][k], obf->pp);
        for (size_t i = 0; i < c->ninputs; i++)
            encoding_mul(obf->enc_vt, obf->pp_vt, tmp, tmp, obf->what[i][inputs[i]][k], obf->pp);

        /* assert(obf_index_eq(obf->pp->toplevel, tmp->index)); */
        /* assert(obf_index_eq(obf->pp->toplevel, outwire->index)); */

        encoding_sub(obf->enc_vt, obf->pp_vt, outwire, outwire, tmp, obf->pp);
        rop[k] = !encoding_is_zero(obf->enc_vt, obf->pp_vt, outwire, obf->pp);

        encoding_free(obf->enc_vt, outwire);
        encoding_free(obf->enc_vt, tmp);
    }
}

////////////////////////////////////////////////////////////////////////////////
// statefully raise encodings to the union of their indices

static void raise_encodings(const obfuscation *const obf, encoding *const x,
                            encoding *const y)
{
    obf_index *target = obf_index_union(obf->enc_vt->mmap_set(x),
                                        obf->enc_vt->mmap_set(y));
    raise_encoding(obf, x, target);
    raise_encoding(obf, y, target);
    obf_index_destroy(target);
}

static void raise_encoding(const obfuscation *const obf, encoding *const x,
                           obf_index *const target)
{
    obf_index *diff_ix = obf_index_difference(target, obf->enc_vt->mmap_set(x));
    for (size_t i = 0; i < obf->op->ninputs; i++) {
        for (size_t b = 0; b <= 1; b++) {
            size_t diff = IX_X(diff_ix, i, b);
            while (diff > 0) {
                // want to find the largest power we obfuscated to multiply by
                size_t p = 0;
                while (((1 << (p+1)) <= diff) && ((p+1) < obf->op->npowers))
                    p++;
                encoding_mul(obf->enc_vt, obf->pp_vt, x, x, obf->uhat[i][b][p], obf->pp);
                diff -= (1 << p);
            }
        }
    }
    size_t diff = IX_Y(diff_ix);
    while (diff > 0) {
        size_t p = 0;
        while (((1 << (p+1)) <= diff) && ((p+1) < obf->op->npowers))
            p++;
        encoding_mul(obf->enc_vt, obf->pp_vt, x, x, obf->vhat[p], obf->pp);
        diff -= (1 << p);
    }
    obf_index_destroy(diff_ix);
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

obfuscator_vtable zim_obfuscator_vtable = {
    .new =  _obfuscator_new,
    .free = _obfuscator_free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _obfuscator_fwrite,
    .fread = _obfuscator_fread,
};
