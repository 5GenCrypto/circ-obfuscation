#include "obfuscator.h"
#include "obf_params.h"
#include "wire.h"
#include "../index_set.h"
#include "../vtables.h"
#include "../util.h"

#include <assert.h>
#include <clt_pl.h>
#include <mmap/mmap_clt_pl.h>
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
    encoding **Chatstar;        /* [m] */
    encoding **zhat;            /* [m] */
    wire_t ***xhat;             /* [n][2] */
    wire_t **yhat;              /* [c] */
    encoding ****what;          /* [n][2][m] */
    long *deg_max;              /* [n] */
};

/* static long * */
/* populate_circ_degrees(const circ_params_t *cp) */
/* { */
/*     long *maxdegs; */
/*     const size_t ninputs = acirc_ninputs(cp->circ); */
/*     const size_t has_consts = acirc_nconsts(cp->circ) ? 1 : 0; */

/*     maxdegs = calloc(ninputs + has_consts, sizeof maxdegs[0]); */
/*     for (size_t i = 0; i < ninputs; ++i) */
/*         maxdegs[i] = acirc_max_var_degree(cp->circ, i); */
/*     if (has_consts) */
/*         maxdegs[ninputs] = acirc_max_const_degree(cp->circ); */
/*     return maxdegs; */
/* } */

static int
populate_circ_inputs(const circ_params_t *cp, ssize_t slot, mpz_t **inputs,
                     mpz_t **consts, const mpz_t *betas)
{
    const size_t ninputs = acirc_ninputs(cp->circ);
    const size_t nconsts = acirc_nconsts(cp->circ);
    for (size_t i = 0; i < ninputs; ++i) {
        if (slot < 0 || i == (size_t) slot)
            mpz_set   (*inputs[i], betas[i]);
        else
            mpz_set_ui(*inputs[i], 1);
    }
    if (consts) {
        for (size_t i = 0; i < nconsts; ++i) {
            if (slot >= 0)
                mpz_set_ui(*consts[i], acirc_const(cp->circ, i));
            else
                mpz_set(*consts[i], betas[ninputs + i]);
        }
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

    encode(args->vt, args->enc, args->slots, args->nslots, args->ix, args->sp, args->level);
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
    const size_t nconsts = acirc_nconsts(cp->circ);
    const size_t noutputs = acirc_noutputs(cp->circ);

    if (obf->Chatstar) {
        for (size_t o = 0; o < noutputs; ++o)
            encoding_free(obf->enc_vt, obf->Chatstar[o]);
        free(obf->Chatstar);
    }
    if (obf->zhat) {
        for (size_t o = 0; o < noutputs; ++o)
            encoding_free(obf->enc_vt, obf->zhat[o]);
        free(obf->zhat);
    }
    if (obf->xhat) {
        for (size_t i = 0; i < ninputs; ++i) {
            for (size_t b = 0; b < 2; ++b)
                wire_free(obf->enc_vt, obf->xhat[i][b]);
            free(obf->xhat[i]);
        }
        free(obf->xhat);
    }
    if (obf->yhat) {
        for (size_t i = 0; i < nconsts; ++i)
            wire_free(obf->enc_vt, obf->yhat[i]);
        free(obf->yhat);
    }
    if (obf->what) {
        for (size_t i = 0; i < ninputs; ++i) {
            for (size_t b = 0; b < 2; ++b) {
                for (size_t o = 0; o < noutputs; ++o)
                    encoding_free(obf->enc_vt, obf->what[i][b][o]);
                free(obf->what[i][b]);
            }
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
    obf->xhat = my_calloc(ninputs, sizeof obf->xhat[0]);
    for (size_t i = 0; i < ninputs; ++i)
        obf->xhat[i] = my_calloc(2, sizeof obf->xhat[0]);
    obf->yhat = my_calloc(nconsts, sizeof obf->yhat[0]);
    obf->zhat = my_calloc(noutputs, sizeof obf->zhat[0]);
    obf->Chatstar = my_calloc(noutputs, sizeof obf->zhat[0]);
    obf->what = my_calloc(ninputs, sizeof obf->what[0]);
    for (size_t i = 0; i < ninputs; ++i) {
        obf->what[i] = my_calloc(2, sizeof obf->what[i][0]);
        for (size_t b = 0; b < 2; ++b)
            obf->what[i][b] = my_calloc(noutputs, sizeof obf->what[i][b][0]);
    }
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
    const size_t nslots = 1 + ninputs + 1;
    const size_t total = obf_num_encodings(cp);

    obfuscation *obf;
    mpz_t *moduli = NULL, *slots = NULL, *alphas = NULL, *betas = NULL;
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
    for (size_t o = 0; o < noutputs; ++o)
        obf->Chatstar[o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            obf->xhat[i][b] = wire_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < nconsts; ++i)
        obf->yhat[i] = wire_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            for (size_t o = 0; o < noutputs; ++o)
                obf->what[i][b][o] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    for (size_t i = 0; i < noutputs; ++i)
        obf->zhat[i] = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

    moduli = mmap->sk->plaintext_fields(obf->sp->sk);
    slots = mpz_vect_new(nslots);
    alphas = mpz_vect_new(ninputs); /* XXX should be α_{i,b} */
    for (size_t i = 0; i < ninputs; ++i)
        mpz_randomm_inv(alphas[i], rng, moduli[1 + i]);
    betas = mpz_vect_new(ninputs + nconsts);
    for (size_t i = 0; i < ninputs + nconsts; ++i)
        mpz_randomm_inv(betas[i], rng, moduli[1 + ninputs]);
    pool = threadpool_create(nthreads);
    pthread_mutex_init(&lock, NULL);

    if (g_verbose)
        print_progress(count, total);

    {   /* Encode \hat x_{i,b} = [ b, 1, ..., 1, α_{i,b}, 1, ..., 1, βᵢ ] */
        for (size_t i = 0; i < ninputs; ++i) {
            for (size_t j = 0; j < ninputs; ++j)
                mpz_set_ui(slots[1 + j], 1);
            for (size_t b = 0; b < 2; ++b) {
                mpz_set_ui(slots[0], b);
                mpz_set(slots[1 + i], alphas[i]);
                mpz_set(slots[1 + ninputs], betas[i]);
                ix = index_set_new(obf_params_nzs(cp));
                __encode(pool, obf->enc_vt, wire_x(obf->xhat[i][b]), nslots, slots,
                         ix, obf->sp, 0, &lock, &count, total);
                mpz_set_ui(slots[0], 1);
                mpz_set_ui(slots[1 + i], 1);
                mpz_set_ui(slots[1 + ninputs], 1);
                ix = index_set_new(obf_params_nzs(cp));
                __encode(pool, obf->enc_vt, wire_u(obf->xhat[i][b]), nslots, slots,
                         ix, obf->sp, 0, &lock, &count, total);
            }
        }
    }

    {   /* Encode \hat y_i = [ b, 1, ..., 1, βᵢ ] */
        for (size_t i = 0; i < nconsts; i++) {
            for (size_t j = 0; j < ninputs; ++j)
                mpz_set_ui(slots[1 + j], 1);
            mpz_set_si(slots[0],           acirc_const(cp->circ, i));
            mpz_set   (slots[1 + ninputs], betas[ninputs + i]);
            ix = index_set_new(obf_params_nzs(cp));
            __encode(pool, obf->enc_vt, wire_x(obf->yhat[i]), nslots, slots,
                     ix, obf->sp, 0, &lock, &count, total);
            mpz_set_ui(slots[0],           1);
            mpz_set_ui(slots[1 + ninputs], 1);
            ix = index_set_new(obf_params_nzs(cp));
            __encode(pool, obf->enc_vt, wire_u(obf->yhat[i]), nslots, slots,
                     ix, obf->sp, 0, &lock, &count, total);
        }
    }

    {   /* Encode \hat w_{i,o} = [0, 1, ..., 1, C†, 1, ..., 1] */
        mpz_t **inputs, **consts;
        mpz_set_ui(slots[0], 0);
        inputs = calloc(ninputs, sizeof inputs[0]);
        for (size_t i = 0; i < ninputs; ++i)
            inputs[i] = mpz_vect_new(1);
        consts = calloc(nconsts, sizeof inputs[0]);
        for (size_t i = 0; i < nconsts; ++i)
            consts[i] = mpz_vect_new(1);
        for (size_t i = 0; i < ninputs; ++i) {
            mpz_t **outputs;
            for (size_t j = 0; j < ninputs + 1; ++j)
                mpz_set_ui(slots[1 + j], 1);
            populate_circ_inputs(cp, i, inputs, consts, alphas);
            outputs = acirc_eval_mpz(cp->circ, inputs, consts, moduli[1 + i]);
            for (size_t b = 0; b < 2; ++b) {
                for (size_t o = 0; o < noutputs; ++o) {
                    mpz_set(slots[1 + i], *outputs[o]);
                    ix = index_set_new(obf_params_nzs(cp));
                    __encode(pool, obf->enc_vt, obf->what[i][b][o], nslots, slots,
                             ix, obf->sp, i /* XXX */, &lock, &count, total);
                }
            }
            for (size_t o = 0; o < noutputs; ++o)
                mpz_vect_free(outputs[o], 1);
            free(outputs);
        }
        for (size_t i = 0; i < nconsts; ++i)
            mpz_vect_free(consts[i], 1);
        free(consts);
        for (size_t i = 0; i < ninputs; ++i)
            mpz_vect_free(inputs[i], 1);
        free(inputs);
    }

    {   /* Encode \hat z_o = [δ_o, 1, ..., 1] */
        for (size_t i = 0; i < ninputs + 1; ++i)
            mpz_set_ui(slots[1 + i], 1);
        for (size_t i = 0; i < noutputs; ++i) {
            ix = index_set_new(obf_params_nzs(cp));
            mpz_randomm_inv(slots[0], rng, moduli[0]);
            __encode(pool, obf->enc_vt, obf->zhat[i], nslots, slots,
                     ix, obf->sp, obf->op->nlevels - 1, &lock, &count, total);
        }
    }

    {   /* Encode \hat C* = [0, 1, ..., 1] */
        mpz_t **inputs, **consts, **outputs;

        inputs = calloc(ninputs, sizeof inputs[0]);
        for (size_t i = 0; i < ninputs; ++i)
            inputs[i] = mpz_vect_new(1);
        consts = calloc(nconsts, sizeof inputs[0]);
        for (size_t i = 0; i < nconsts; ++i)
            consts[i] = mpz_vect_new(1);
        populate_circ_inputs(cp, -1, inputs, consts, betas);
        outputs = acirc_eval_mpz(cp->circ, inputs, consts, moduli[1 + ninputs]);

        mpz_set_ui(slots[0], 0);
        for (size_t i = 0; i < ninputs; ++i) {
            mpz_set_ui(slots[1 + i], 1);
        }
        for (size_t o = 0; o < noutputs; ++o) {
            mpz_set(slots[1 + ninputs], *outputs[o]);
            ix = index_set_new(obf_params_nzs(cp));
            __encode(pool, obf->enc_vt, obf->Chatstar[o], nslots, slots,
                     ix, obf->sp, 0, &lock, &count, total);
        }
        for (size_t i = 0; i < nconsts; ++i)
            mpz_vect_free(consts[i], 1);
        free(consts);
        for (size_t i = 0; i < ninputs; ++i)
            mpz_vect_free(inputs[i], 1);
        free(inputs);
        for (size_t i = 0; i < noutputs; ++i)
            mpz_vect_free(outputs[i], 1);
        free(outputs);
    }


    result = OK;
cleanup:
    mpz_vect_free(slots, nslots);
    mpz_vect_free(alphas, ninputs);
    mpz_vect_free(betas, ninputs + nconsts);
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
    for (size_t o = 0; o < noutputs; ++o)
        encoding_fwrite(obf->enc_vt, obf->Chatstar[o], fp);
    for (size_t i = 0; i < noutputs; ++i)
        encoding_fwrite(obf->enc_vt, obf->zhat[i], fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            wire_fwrite(obf->enc_vt, obf->xhat[i][b], fp);
    for (size_t i = 0; i < nconsts; ++i)
        wire_fwrite(obf->enc_vt, obf->yhat[i], fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            for (size_t o = 0; o < noutputs; ++o)
                encoding_fwrite(obf->enc_vt, obf->what[i][b][o], fp);
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
    for (size_t o = 0; o < noutputs; ++o)
        obf->Chatstar[o] = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < noutputs; ++i)
        obf->zhat[i] = encoding_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            obf->xhat[i][b] = wire_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < nconsts; ++i)
        obf->yhat[i] = wire_fread(obf->enc_vt, fp);
    for (size_t i = 0; i < ninputs; ++i)
        for (size_t b = 0; b < 2; ++b)
            for (size_t o = 0; o < noutputs; ++o)
                obf->what[i][b][o] = encoding_fread(obf->enc_vt, fp);
    return obf;
}

typedef struct {
    const obfuscation *obf;
    const long *inputs;
    switch_state_t ***switches;
} eval_args_t;

static void *
copy_f(void *x, void *args_)
{
    eval_args_t *args = args_;
    const obfuscation *obf = args->obf;
    wire_t *out;

    out = wire_new(obf->enc_vt, obf->pp_vt, obf->pp);
    wire_set(obf->enc_vt, out, x);
    return out;
}

static void *
input_f(size_t ref, size_t i, void *args_)
{
    (void) ref;
    eval_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    return copy_f(obf->xhat[i][args->inputs[i]], args_);
}

static void *
const_f(size_t ref, size_t i, long val, void *args_)
{
    (void) ref; (void) val;
    eval_args_t *args = args_;
    const obfuscation *const obf = args->obf;
    return copy_f(obf->yhat[i], args_);
}

static void *
eval_f(size_t ref, acirc_op op, size_t xref, const void *x_, size_t yref, const void *y_, void *args_)
{
    (void) xref; (void) yref;
    eval_args_t *args = args_;
    const obfuscation *obf = args->obf;
    const wire_t *x = x_;
    const wire_t *y = y_;
    wire_t *res;

    res = wire_new(obf->enc_vt, obf->pp_vt, obf->pp);
    switch (op) {
    case ACIRC_OP_MUL:
        if (wire_mul(obf->enc_vt, obf->pp_vt, obf->pp, res, x, y,
                     args->switches ? args->switches[ref] : NULL) == ERR)
            goto error;
        break;
    case ACIRC_OP_ADD:
        wire_add(obf->enc_vt, obf->pp_vt, obf->pp, res, x, y,
                 args->switches ? args->switches[ref] : NULL);
        break;
    case ACIRC_OP_SUB:
        wire_sub(obf->enc_vt, obf->pp_vt, obf->pp, res, x, y,
                 args->switches ? args->switches[ref] : NULL);
        break;
    }
    return res;
error:
    wire_free(obf->enc_vt, res);
    return NULL;
}

static void *
output_f(size_t ref, size_t o, void *x_, void *args_)
{
    (void) ref;
    long output = 1;
    eval_args_t *args = args_;
    const obfuscation *obf = args->obf;
    const circ_params_t *const cp = &obf->op->cp;
    const size_t ninputs = acirc_ninputs(cp->circ);
    encoding *out, *lhs, *rhs;
    const index_set *const toplevel = obf->pp_vt->toplevel(obf->pp);
    wire_t *x = x_;

    out = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    lhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);
    rhs = encoding_new(obf->enc_vt, obf->pp_vt, obf->pp);

    /* Compute LHS */
    ref = acirc_nrefs(cp->circ) + o * (ninputs + 2);
    if (obf->mmap == &clt_pl_vtable)
        clt_pl_elem_switch(wire_x(x)->enc, obf->pp->pp, wire_x(x)->enc, args->switches[ref][0]);
    encoding_mul(obf->enc_vt, obf->pp_vt, lhs, wire_x(x), obf->zhat[o], obf->pp);
    if (obf->mmap == &clt_pl_vtable)
        clt_pl_elem_switch(lhs->enc, obf->pp->pp, lhs->enc, args->switches[ref][1]);
    if (!index_set_eq(obf->enc_vt->mmap_set(lhs), toplevel)) {
        fprintf(stderr, "error: lhs != toplevel\n");
        index_set_print(obf->enc_vt->mmap_set(lhs));
        index_set_print(toplevel);
        goto cleanup;
    }
    /* Compute RHS */
    ref++;
    encoding_set(obf->enc_vt, rhs, obf->Chatstar[o]);
    for (size_t i = 0; i < ninputs; ++i) {
        /* XXX wrong */
        encoding_mul(obf->enc_vt, obf->pp_vt, rhs, rhs, args->obf->what[i][0][o], obf->pp);
        if (obf->mmap == &clt_pl_vtable)
            clt_pl_elem_switch(rhs->enc, obf->pp->pp, rhs->enc, args->switches[ref++][1]);
    }
    if (!index_set_eq(obf->enc_vt->mmap_set(rhs), toplevel)) {
        fprintf(stderr, "error: rhs != toplevel\n");
        index_set_print(obf->enc_vt->mmap_set(rhs));
        index_set_print(toplevel);
        goto cleanup;
    }
    if (obf->mmap == &clt_pl_vtable)
        clt_pl_elem_switch(rhs->enc, obf->pp->pp, rhs->enc, args->switches[ref++][1]);
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
        wire_free(args->obf->enc_vt, x);
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
        eval_args_t args = {
            .obf = obf,
            .inputs = inputs,
            .switches = NULL,
        };
        if (obf->mmap == &clt_pl_vtable)
            args.switches = clt_pl_pp_switches(obf->pp->pp);
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
