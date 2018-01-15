#include "obfuscator.h"
#include "obf_params.h"
#include "../index_set.h"
#include "../vtables.h"
#include "../util.h"

#include <assert.h>
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
    size_t npowers;
    encoding *Chatstar;
    encoding *zhat;
    encoding ***xhat;           /* [n][2] */
    encoding **yhat;            /* [c] */
    encoding ***what;           /* [n][m] */
    encoding **uhat;            /* [n] */
    long *deg_max;              /* [n] */
};

size_t
obf_num_encodings(const circ_params_t *cp, size_t npowers)
{
    (void) npowers;             /* XXX */
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);
    return 1 + 1 + ninputs * 2 + ninputs * noutputs + nconsts + ninputs;
}

static long *
populate_circ_degrees(const circ_params_t *cp)
{
    long *maxdegs;
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0;

    maxdegs = calloc(ninputs + has_consts, sizeof maxdegs[0]);
    for (size_t i = 0; i < ninputs; ++i)
        maxdegs[i] = acirc_max_var_degree(cp->circ, i);
    if (has_consts)
        maxdegs[ninputs] = acirc_max_const_degree(cp->circ);
    return maxdegs;
}

static int
populate_circ_inputs(const circ_params_t *cp, size_t slot, mpz_t **inputs,
                     const mpz_t *alphas)
{
    for (size_t i = 0; i < acirc_ninputs(cp->circ); ++i) {
        if (i == slot)
            mpz_set   (*inputs[i], alphas[i]);
        else
            mpz_set_ui(*inputs[i], 1);
    }
    return OK;
}

typedef struct {
    const encoding_vtable *vt;
    encoding *enc;
    size_t level;
    mpz_t *slots;
    size_t nslots;
    index_set *ix;
    const secret_params *sp;
    pthread_mutex_t *lock;
    size_t *count;
    size_t total;
} obf_args_t;


static void
encode_worker(void *wargs)
{
    obf_args_t *const args = wargs;

    encode(args->vt, args->enc, args->slots, args->nslots, args->ix,
           args->sp, args->level);
    if (g_verbose) {
        pthread_mutex_lock(args->lock);
        print_progress(++*args->count, args->total);
        pthread_mutex_unlock(args->lock);
    }
    mpz_vect_free(args->slots, args->nslots);
    index_set_free(args->ix);
    free(args);
}

static void
__encode(threadpool *pool, const encoding_vtable *vt, encoding *enc, size_t nslots,
         mpz_t slots[nslots], index_set *ix, const secret_params *sp, size_t level,
         pthread_mutex_t *lock, size_t *count, size_t total)
{
    obf_args_t *args;
    args = my_calloc(1, sizeof args[0]);
    args->vt = vt;
    args->enc = enc;
    args->level = level;
    args->slots = my_calloc(nslots, sizeof args->slots[0]);
    for (size_t i = 0; i < nslots; ++i)
        mpz_init_set(args->slots[i], slots[i]);
    args->nslots = nslots;
    args->ix = ix;
    args->sp = sp;
    args->lock = lock;
    args->count = count;
    args->total = total;
    threadpool_add_job(pool, encode_worker, args);
}

static void
_free(obfuscation *obf)
{
    if (obf == NULL)
        return;

    const circ_params_t *cp = &obf->op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);

    if (obf->Chatstar)
        encoding_free(obf->enc_vt, obf->Chatstar);
    if (obf->zhat)
        encoding_free(obf->enc_vt, obf->zhat);
    if (obf->uhat) {
        for (size_t i = 0; i < ninputs; ++i)
            encoding_free(obf->enc_vt, obf->uhat[i]);
        free(obf->uhat);
    }
    if (obf->xhat) {
        for (size_t i = 0; i < ninputs; ++i) {
            for (size_t b = 0; b < 2; ++b)
                encoding_free(obf->enc_vt, obf->xhat[i][b]);
            free(obf->xhat[i]);
        }
        free(obf->xhat);
    }
    if (obf->what) {
        for (size_t i = 0; i < ninputs; ++i) {
            for (size_t o = 0; o < noutputs; ++o)
                encoding_free(obf->enc_vt, obf->what[i][o]);
            free(obf->what[i]);
        }
        free(obf->what);
    }
    if (obf->pp)
        public_params_free(obf->pp_vt, obf->pp);
    if (obf->sp)
        secret_params_free(obf->sp_vt, obf->sp);
    free(obf);
}

static obfuscation *
_alloc(const mmap_vtable *mmap, const obf_params_t *op)
{
    obfuscation *obf;

    const circ_params_t *cp = &op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);

    if ((obf = my_calloc(1, sizeof obf[0])) == NULL)
        return NULL;
    obf->mmap = mmap;
    obf->op = op;
    obf->enc_vt = get_encoding_vtable(mmap);
    obf->pp_vt = get_pp_vtable(mmap);
    obf->sp_vt = get_sp_vtable(mmap);
    obf->uhat = my_calloc(ninputs, sizeof obf->uhat[0]);
    obf->xhat = my_calloc(ninputs, sizeof obf->xhat[0]);
    for (size_t i = 0; i < ninputs; ++i)
        obf->xhat[i] = my_calloc(2, sizeof obf->xhat[0]);
    obf->yhat = my_calloc(nconsts, sizeof obf->yhat[0]);
    obf->what = my_calloc(ninputs, sizeof obf->what[0]);
    for (size_t i = 0; i < ninputs; ++i)
        obf->what[i] = my_calloc(noutputs, sizeof obf->what[i][0]);

    return obf;
}

static obfuscation *
_obfuscate(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    (void) kappa;
    const circ_params_t *cp = &op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);
    const size_t nslots = 1 + ninputs;
    const size_t total = obf_num_encodings(cp, op->npowers);

    obfuscation *obf;
    mpz_t *moduli = NULL, *slots, *alphas;
    threadpool *pool = NULL;
    index_set *ix = NULL;
    pthread_mutex_t lock;
    size_t count = 0;
    int result = ERR;

    if ((obf = _alloc(mmap, op)) == NULL)
        return NULL;
    if ((obf->sp = secret_params_new(obf->sp_vt, op, secparam, 0, nthreads, rng)) == NULL)
        goto cleanup;
    if ((obf->pp = public_params_new(obf->pp_vt, obf->sp_vt, obf->sp)) == NULL)
        goto cleanup;
    obf->npowers = op->npowers;
    obf->Chatstar = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < ninputs; ++i)
        obf->uhat[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            obf->xhat[i][b] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < nconsts; ++i)
        obf->yhat[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t o = 0; o < noutputs; ++o)
            obf->what[i][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    obf->zhat = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

    moduli = mmap->sk->plaintext_fields(obf->sp->sk);
    slots = mpz_vect_new(nslots);
    alphas = mpz_vect_new(ninputs);
    for (size_t i = 0; i < ninputs; ++i)
        mpz_randomm_inv(alphas[i], rng, moduli[1 + i]); /* XXX modulus correct? */
    pool = threadpool_create(nthreads);
    pthread_mutex_init(&lock, NULL);

    if (g_verbose)
        print_progress(count, total);

    {   /* Encode \hat C* = [0, 1, ..., 1] */
        long *maxdegs;

        maxdegs = populate_circ_degrees(cp);
        ix = index_set_new(obf_params_nzs(cp));
        mpz_set_ui(slots[0], 0);
        for (size_t i = 0; i < ninputs; ++i) {
            mpz_set_ui(slots[1 + i], 1);
            /* IX_X(ix, cp, i) = maxdegs[i]; */
        }
        /* IX_Z(ix) = 1; */
        __encode(pool, obf->enc_vt, obf->Chatstar, nslots, slots,
                 ix, obf->sp, 0, &lock, &count, total);
        free(maxdegs);
    }

    {   /* Encode \hat uᵢ = [1, ..., 1] */
        for (size_t i = 0; i < nslots; ++i)
            mpz_set_ui(slots[i], 1);
        for (size_t i = 0; i < ninputs; ++i) {
            ix = index_set_new(obf_params_nzs(cp));
            /* IX_X(ix, cp, i) = 1; */
            __encode(pool, obf->enc_vt, obf->uhat[i], nslots, slots,
                     ix, obf->sp, 0, &lock, &count, total);
        }
    }

    {   /* Encode \hat x_{i,b} = [b, 1, ..., 1, αᵢ, 1, ..., 1] */
        for (size_t i = 0; i < ninputs; ++i) {
            for (size_t j = 0; j < ninputs; ++j)
                mpz_set_ui(slots[1 + j], 1);
            mpz_set(slots[1 + i], alphas[i]);
            for (size_t b = 0; b < 2; ++b) {
                mpz_set_ui(slots[0], b);
                ix = index_set_new(obf_params_nzs(cp));
                /* IX_X(ix, cp, i) = 1; */
                __encode(pool, obf->enc_vt, obf->xhat[i][b], nslots, slots,
                         ix, obf->sp, 0, &lock, &count, total);
            }
        }
    }

    {   /* Encode \hat y_i = [b, 1, ..., 1, βᵢ, 1, ..., 1] */
        for (size_t i = 0; i < nconsts; i++) {
            ix = index_set_new(obf_params_nzs(cp));
            /* IX_Y(ix, cp) = 1; */
            mpz_set_si(slots[0], acirc_const(cp->circ, i));
            for (size_t j = 0; j < ninputs; ++j)
                mpz_set_ui(slots[1 + j], 1);
            mpz_set_ui(slots[1 + i], 0); /* XXX */
            __encode(pool, obf->enc_vt, obf->yhat[i], nslots, slots,
                     ix, obf->sp, 0, &lock, &count, total);
        }
    }

    {   /* Encode \hat w_{i,o} = [0, 1, ..., 1, C†, 1, ..., 1] */
        mpz_t **inputs;
        mpz_set_ui(slots[0], 0);
        inputs = calloc(ninputs, sizeof inputs[0]);
        for (size_t i = 0; i < ninputs; ++i)
            inputs[i] = mpz_vect_new(1);
        for (size_t i = 0; i < ninputs; ++i) {
            mpz_t **outputs;
            for (size_t j = 0; j < ninputs; ++j)
                mpz_set_ui(slots[1 + j], 1);
            populate_circ_inputs(cp, i, inputs, alphas);
            outputs = acirc_eval_mpz(cp->circ, inputs, NULL, moduli[i]);
            for (size_t o = 0; o < noutputs; ++o) {
                mpz_set(slots[1 + i], *outputs[o]);
                ix = index_set_new(obf_params_nzs(cp));
                /* IX_W(ix, cp, i) = 1; */
                __encode(pool, obf->enc_vt, obf->what[i][o], nslots, slots,
                         ix, obf->sp, i /* XXX */, &lock, &count, total);
                mpz_vect_free(outputs[o], 1);
            }
            free(outputs);
        }
        for (size_t i = 0; i < ninputs; ++i)
            mpz_vect_free(inputs[i], 1);
        free(inputs);
    }

    {   /* Encode \hat z = [δ, 1, ..., 1] */
        ix = index_set_new(obf_params_nzs(cp));
        mpz_randomm_inv(slots[0], rng, moduli[0]);
        for (size_t i = 0; i < ninputs; ++i) {
            mpz_set_ui(slots[1 + i], 1);
            /* IX_W(ix, cp, i) = 1; */
        }
        /* IX_Z(ix) = 1; */
        __encode(pool, obf->enc_vt, obf->zhat, nslots, slots,
                 ix, obf->sp, 0, &lock, &count, total);
    }

    result = OK;
cleanup:
    mpz_vect_free(slots, nslots);
    mpz_vect_free(alphas, ninputs);
    /* mpz_vect_free(moduli, mmap->sk->nslots(obf->sp->sk)); */
    threadpool_destroy(pool);
    pthread_mutex_destroy(&lock);
    if (result == OK)
        return obf;
    else {
        _free(obf);
        return NULL;
    }
}

static int
_fwrite(const obfuscation *obf, FILE *fp)
{
    const circ_params_t *cp = &obf->op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);

    public_params_fwrite(obf->pp_vt, obf->pp, fp);
    encoding_fwrite(obf->enc_vt, obf->Chatstar, fp);
    encoding_fwrite(obf->enc_vt, obf->zhat, fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            encoding_fwrite(obf->enc_vt, obf->xhat[i][b], fp);
    for (size_t i = 0; i < nconsts; ++i)
        encoding_fwrite(obf->enc_vt, obf->yhat[i], fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t o = 0; o < noutputs; ++o)
            encoding_fwrite(obf->enc_vt, obf->what[i][o], fp);
    for (size_t i = 0; i < ninputs; ++i)
        encoding_fwrite(obf->enc_vt, obf->uhat[i], fp);
    return OK;
}

static obfuscation *
_fread(const mmap_vtable *mmap, const obf_params_t *op, FILE *fp)
{
    obfuscation *obf;
    const circ_params_t *cp = &op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);

    if ((obf = _alloc(mmap, op)) == NULL)
        return NULL;

    obf->pp = public_params_fread(obf->pp_vt, op, fp);
    obf->Chatstar = encoding_fread(obf->enc_vt, fp);
    obf->zhat = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            obf->xhat[i][b] = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < nconsts; ++i)
        obf->yhat[i] = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t o = 0; o < noutputs; ++o)
            obf->what[i][o] = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < ninputs; ++i)
        obf->uhat[i] = encoding_fread(obf->enc_vt, fp);
    return obf;
}

static void
_raise_encoding(const obfuscation *obf, encoding *x, encoding *u, size_t diff)
{
    while (diff > 0) {
        /* size_t p = 0; */
        /* while (((size_t) 1 << (p+1)) <= diff && (p+1) < obf->npowers) */
        /*     p++; */
        /* encoding_mul(obf->enc_vt, obf->pp_vt, x, x, us[p], obf->pp); */
        /* diff -= (1 << p); */
        encoding_mul(obf->enc_vt, obf->pp_vt, x, x, u, obf->pp, 0); /* XXX */
        diff -= 1;
    }
}

static int
raise_encoding(const obfuscation *obf, encoding *x, const index_set *target)
{
    const circ_params_t *const cp = &obf->op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    index_set *ix;
    size_t diff;

    if ((ix = index_set_difference(target, obf->enc_vt->mmap_set(x))) == NULL)
        return ERR;
    for (size_t i = 0; i < ninputs; ++i) {
        diff = IX_X(ix, cp, i);
        if (diff > 0)
            _raise_encoding(obf, x, obf->uhat[i], diff);
    }
    index_set_free(ix);
    return OK;
}

static int
raise_encodings(const obfuscation *obf, encoding *x, encoding *y)
{
    index_set *ix;
    int ret = ERR;
    ix = index_set_union(obf->enc_vt->mmap_set(x), obf->enc_vt->mmap_set(y));
    if (ix == NULL)
        goto cleanup;
    if (raise_encoding(obf, x, ix) == ERR)
        goto cleanup;
    if (raise_encoding(obf, y, ix) == ERR)
        goto cleanup;
    ret = OK;
cleanup:
    if (ix)
        index_set_free(ix);
    return ret;
}

typedef struct {
    obfuscation *obf;
    long *inputs;
    size_t count;
} eval_args_t;

static void *
copy_f(void *x, void *args_)
{
    eval_args_t *args = args_;
    obfuscation *obf = args->obf;
    encoding *out;

    out = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    encoding_set(obf->enc_vt, out, x);
    return out;
}

static void *
input_f(size_t i, void *args_)
{
    eval_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    return copy_f(obf->xhat[i][0], args_);
}

static void *
const_f(size_t i, long val, void *args_)
{
    (void) val;
    eval_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    return copy_f(obf->yhat[i], args_);
}

static void *
eval_f(acirc_op op, const void *x_, const void *y_, void *args_)
{
    eval_args_t *args = args_;
    obfuscation *obf = args->obf;
    const encoding *x = x_;
    const encoding *y = y_;
    encoding *res;

    res = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    switch (op) {
    case ACIRC_OP_MUL:
        encoding_mul(obf->enc_vt, obf->pp_vt, res, x, y, obf->pp, args->count++);
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
        } else {
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
output_f(size_t o, void *x, void *args_)
{
    long output = 1;
    eval_args_t *args = args_;
    obfuscation *obf = args->obf;
    const circ_params_t *const cp = &obf->op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    encoding *out, *lhs, *rhs;
    const index_set *const toplevel = obf->pp_vt->toplevel(obf->pp);

    out = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    lhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    rhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

    /* Compute LHS */
    encoding_mul(obf->enc_vt, obf->pp_vt, lhs, x, obf->zhat, obf->pp, args->count++);
    /* raise_encoding(obf, lhs, toplevel); */
    if (!index_set_eq(obf->enc_vt->mmap_set(lhs), toplevel)) {
        fprintf(stderr, "error: lhs != toplevel\n");
        index_set_print(obf->enc_vt->mmap_set(lhs));
        index_set_print(toplevel);
        goto cleanup;
    }
    /* Compute RHS */
    encoding_set(obf->enc_vt, rhs, obf->Chatstar);
    for (size_t i = 0; i < ninputs; ++i)
        encoding_mul(obf->enc_vt, obf->pp_vt, rhs, rhs, args->obf->what[i][o], obf->pp, args->count++);
    if (!index_set_eq(obf->enc_vt->mmap_set(rhs), toplevel)) {
        fprintf(stderr, "error: rhs != toplevel\n");
        index_set_print(obf->enc_vt->mmap_set(rhs));
        index_set_print(toplevel);
        goto cleanup;
    }
    encoding_sub(obf->enc_vt, obf->pp_vt, out, lhs, rhs, obf->pp);
    output = !encoding_is_zero(obf->enc_vt, obf->pp_vt, out, obf->pp);
cleanup:
    encoding_free(obf->enc_vt, out);
    encoding_free(obf->enc_vt, lhs);
    encoding_free(obf->enc_vt, rhs);
    return (void *) output;
}

static void
free_f(void *x, void *args_)
{
    eval_args_t *args = args_;
    if (x)
        encoding_free(args->obf->enc_vt, x);
}

static int
_evaluate(const obfuscation *obf, long *outputs, size_t noutputs,
          const long *inputs, size_t ninputs, size_t nthreads,
          size_t *kappa, size_t *npowers)
{
    (void) kappa; (void) npowers;
    const circ_params_t *cp = &obf->op->cp;

    if (ninputs != acirc_ninputs(cp->circ)) {
        fprintf(stderr, "error: obf evaluate: invalid number of inputs\n");
        return ERR;
    }
    if (noutputs != acirc_noutputs(cp->circ)) {
        fprintf(stderr, "error: obf evaluate: invalid number of outputs\n");
        return ERR;
    }

    {
        long *tmp;
        eval_args_t args = {.obf = obf, .inputs = inputs, .count = 0 };
        tmp = (long *) acirc_traverse(cp->circ, input_f, const_f, eval_f,
                                      output_f, free_f, &args, nthreads);
        if (outputs)
            for (size_t i = 0; i < acirc_noutputs(cp->circ); ++i)
                outputs[i] = tmp[i];
        free(tmp);
    }

    return OK;
}

obfuscator_vtable polylog_obfuscator_vtable = {
    .free = _free,
    .obfuscate = _obfuscate,
    .evaluate = _evaluate,
    .fwrite = _fwrite,
    .fread = _fread,
};

