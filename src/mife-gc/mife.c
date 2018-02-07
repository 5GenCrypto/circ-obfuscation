#include "mife.h"
#include "mife_params.h"
#include "../mife-cmr/mife.h"
#include "../mife_util.h"
#include "../util.h"

struct mife_t {
    const mmap_vtable *mmap;
    const mife_vtable *vt;
    mife_cmr_mife_t *mife;
    const obf_params_t *op;
    char *dirname;
    obf_params_t *op_;
    acirc_t *gc;
};

struct mife_sk_t {
    const mife_vtable *vt;
    mife_cmr_mife_sk_t *sk;
    acirc_t *gc;
    const acirc_t *circ;
    char *dirname;
    size_t npowers;
    size_t wirelen;
    size_t padding;
    bool local;
};

struct mife_ek_t {
    const mmap_vtable *mmap;
    const mife_vtable *vt;
    mife_cmr_mife_ek_t *ek;
    const acirc_t *circ;
    char *dirname;
    acirc_t *gc;
    size_t npowers;
    size_t padding;
    bool local;
};

struct mife_ct_t {
    const mife_vtable *vt;
    mife_cmr_mife_ct_t *ct;
    const acirc_t *gc;
    /* encoding **seed;            /\* [λ] *\/ */
    /* XXX wire labels */
};

static void
mife_ct_free(mife_ct_t *ct)
{
    if (ct == NULL) return;
    if (ct->ct)
        ct->vt->mife_ct_free(ct->ct);
    free(ct);
}

static int
mife_ct_fwrite(const mife_ct_t *ct, FILE *fp)
{
    if (ct == NULL) return ERR;
    return ct->vt->mife_ct_fwrite(ct->ct, fp);
}

static mife_ct_t *
mife_ct_fread(const mmap_vtable *mmap, const mife_ek_t *ek, FILE *fp)
{
    mife_ct_t *ct;

    if ((ct = my_calloc(1, sizeof ct[0])) == NULL)
        return NULL;
    ct->vt = &mife_cmr_vtable;
    ct->gc = ek->gc;
    ct->ct = ct->vt->mife_ct_fread(mmap, ek->ek, fp);
    return ct;
}

static mife_sk_t *
mife_sk(const mife_t *mife)
{
    mife_sk_t *sk;
    if ((sk = my_calloc(1, sizeof sk[0])) == NULL)
        return NULL;
    sk->vt = &mife_cmr_vtable;
    sk->circ = mife->op->circ;
    sk->gc = mife->gc;
    sk->dirname = mife->dirname;
    if ((sk->sk = mife->vt->mife_sk(mife->mife)) == NULL)
        goto error;
    sk->npowers = mife->op->npowers;
    sk->padding = mife->op->padding;
    sk->wirelen = mife->op->wirelen;
    sk->local = false;
    return sk;
error:
    if (sk)
        free(sk);
    return NULL;
}

static void
mife_sk_free(mife_sk_t *sk)
{
    if (sk == NULL) return;
    sk->vt->mife_sk_free(sk->sk);
    if (sk->local) {
        free(sk->dirname);
        acirc_free(sk->gc);
    }
    free(sk);
}

static int
mife_sk_fwrite(const mife_sk_t *sk, FILE *fp)
{
    if (sk == NULL) return ERR;
    if (str_fwrite(sk->dirname, fp) == ERR) return ERR;
    if (sk->vt->mife_sk_fwrite(sk->sk, fp) == ERR) return ERR;
    if (size_t_fwrite(sk->npowers, fp) == ERR) return ERR;
    if (size_t_fwrite(sk->padding, fp) == ERR) return ERR;
    if (size_t_fwrite(sk->wirelen, fp) == ERR) return ERR;
    return OK;
}

static mife_sk_t *
mife_sk_fread(const mmap_vtable *mmap, const acirc_t *circ, FILE *fp)
{
    mife_sk_t *sk = NULL;
    char *fname = NULL;
    if ((sk = my_calloc(1, sizeof sk[0])) == NULL)
        return NULL;
    sk->vt = &mife_cmr_vtable;
    sk->circ = circ;
    if ((sk->dirname = str_fread(fp)) == NULL) goto error;
    if ((fname = makestr("%s/gb.acirc2", sk->dirname)) == NULL) goto error;
    if ((sk->gc = acirc_new(fname, false, true)) == NULL) goto error;
    if ((sk->sk = sk->vt->mife_sk_fread(mmap, sk->gc, fp)) == NULL) goto error;
    if (size_t_fread(&sk->npowers, fp) == ERR) goto error;
    if (size_t_fread(&sk->padding, fp) == ERR) goto error;
    if (size_t_fread(&sk->wirelen, fp) == ERR) goto error;
    sk->local = true;
    if (fname)
        free(fname);
    return sk;
error:
    if (fname)
        free(fname);
    if (sk)
        mife_sk_free(sk);
    return NULL;
}

static mife_ek_t *
mife_ek(const mife_t *mife)
{
    mife_ek_t *ek;
    if ((ek = my_calloc(1, sizeof ek[0])) == NULL)
        return NULL;
    ek->mmap = mife->mmap;
    ek->vt = mife->vt;
    ek->ek = mife->vt->mife_ek(mife->mife);
    ek->dirname = mife->dirname;
    ek->circ = mife->op->circ;
    ek->npowers = mife->op->npowers;
    ek->padding = mife->op->padding;
    ek->local = false;
    return ek;
}

static void
mife_ek_free(mife_ek_t *ek)
{
    if (ek == NULL) return;
    ek->vt->mife_ek_free(ek->ek);
    if (ek->local) {
        free(ek->dirname);
        acirc_free(ek->gc);
    }
    free(ek);
}

static int
mife_ek_fwrite(const mife_ek_t *ek, FILE *fp)
{
    if (str_fwrite(ek->dirname, fp) == ERR) return ERR;
    if (ek->vt->mife_ek_fwrite(ek->ek, fp) == ERR) return ERR;
    if (size_t_fwrite(ek->npowers, fp) == ERR) return ERR;
    if (size_t_fwrite(ek->padding, fp) == ERR) return ERR;
    return OK;
}

static mife_ek_t *
mife_ek_fread(const mmap_vtable *mmap, const acirc_t *circ, FILE *fp)
{
    mife_ek_t *ek;
    char *fname = NULL;
    if ((ek = my_calloc(1, sizeof ek[0])) == NULL)
        return NULL;
    ek->mmap = mmap;
    ek->vt = &mife_cmr_vtable;
    ek->circ = circ;
    if ((ek->dirname = str_fread(fp)) == NULL) goto error;
    if ((fname = makestr("%s/gb.acirc2", ek->dirname)) == NULL) goto error;
    if ((ek->gc = acirc_new(fname, false, true)) == NULL) goto error;
    if ((ek->ek = ek->vt->mife_ek_fread(mmap, ek->gc, fp)) == NULL) goto error;
    if (size_t_fread(&ek->npowers, fp) == ERR) goto error;
    if (size_t_fread(&ek->padding, fp) == ERR) goto error;
    ek->local = true;
    if (fname)
        free(fname);
    return ek;
error:
    if (fname)
        free(fname);
    if (ek)
        mife_ek_free(ek);
    return NULL;
}

static void
mife_free(mife_t *mife)
{
    if (mife == NULL) return;
    if (mife->mife)
        mife->vt->mife_free(mife->mife);
    if (mife->op_)
        mife_cmr_op_vtable.free(mife->op_);
    if (mife->gc)
        acirc_free(mife->gc);
    if (mife->dirname)
        free(mife->dirname);
    free(mife);
}

static mife_ct_t *
mife_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
             size_t ninputs, size_t nthreads, aes_randstate_t rng);

static mife_t *
mife_setup(const mmap_vtable *mmap, const obf_params_t *op, size_t secparam,
           size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    mife_t *mife;

    if (!acirc_is_binary(op->circ)) {
        fprintf(stderr, "%s: GC approach only works for binary circuits\n",
                errorstr);
        return NULL;
    }

    if (g_verbose)
        fprintf(stderr, "Running MIFE setup:\n");

    if ((mife = my_calloc(1, sizeof mife[0])) == NULL)
        return NULL;
    mife->op = op;
    mife->vt = &mife_cmr_vtable;
    mife->mmap = mmap;
    if ((mife->dirname = makestr("%s.gc", acirc_fname(op->circ))) == NULL)
        goto error;
    /* generate garbled circuit for input circuit */
    {
        const double start = current_time();
        char *cmd = NULL;

        if (g_verbose)
            fprintf(stderr, "— Generating garbled circuit: ");

        (void) makedir(mife->dirname);
        if ((cmd = makestr("boots garble \"%s\" -d %s -p %lu -s %lu",
                           acirc_fname(op->circ), mife->dirname,
                           op->padding, op->wirelen)) == NULL) {
            goto error;
        }
        if (system(cmd) != 0) {
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: error calling '%s'\n",
                    errorstr, __func__, cmd);
            free(cmd);
            goto error;
        }
        free(cmd);
        if (g_verbose)
            fprintf(stderr, "%.2f s\n", current_time() - start);
    }
    /* run MIFE setup on garbled circuit */
    {
        const double start = current_time();
        char *fname;

        if ((fname = makestr("%s/gb.acirc2", mife->dirname)) == NULL)
            goto error;

        if (g_verbose)
            fprintf(stderr, "— Running MIFE setup on garbled circuit:\n");

        if ((mife->gc = acirc_new(fname, false, true)) == NULL) {
            fprintf(stderr, "%s: %s: unable to parse '%s'\n",
                    errorstr, __func__, fname);
            free(fname);
            goto error;
        }
        free(fname);
        if (g_verbose)
            circ_params_print(mife->gc);
        {
            mife_cmr_params_t params = { .npowers = mife->op->npowers };
            mife->op_ = mife_cmr_op_vtable.new(mife->gc, &params);
        }
        if ((mife->mife = mife->vt->mife_setup(mmap, mife->op_, secparam, kappa,
                                               nthreads, rng)) == NULL)
            goto error;
        if (g_verbose)
            fprintf(stderr, "— MIFE setup: %.2f s\n", current_time() - start);
    }
    /* encrypt each index */
    {
        const double start = current_time();
        const size_t nindices = acirc_symlen(mife->gc, 1); /* XXX */
        mife_sk_t *sk;
        int ret = OK;
        sk = mife_sk(mife);
        if (g_verbose)
            fprintf(stderr, "— Encrypting %lu indices:\n",
                    nindices);
        for (size_t i = 0; i < nindices; ++i) {
            char *ctname = NULL;
            mife_cmr_mife_ct_t *ct = NULL;
            long *input = NULL;

            if (g_verbose)
                fprintf(stderr, "—— Index #%lu\n", i);
            if ((input = my_calloc(nindices, sizeof input[0])) == NULL)
                goto cleanup;
            input[i] = 1;
            if ((ct = my_calloc(1, sizeof ct[0])) == NULL)
                goto cleanup;
            ct->vt = &mife_cmr_vtable;
            ct->gc = sk->gc;
            if ((ct->ct = mife->vt->mife_encrypt(sk->sk, 1, input, nindices,
                                                 nthreads, rng)) == NULL) {
                ret = ERR;
                goto cleanup;
            }
            ctname = makestr("%s.ix%lu.1.ct", acirc_fname(mife->gc), i);
            mife_ct_write(&mife_gc_vtable, ct, ctname);
        cleanup:
            if (ct)
                mife_ct_free(ct);
            if (ctname)
                free(ctname);
            if (input)
                free(input);
            if (ret == ERR) {
                mife_sk_free(sk);
                goto error;
            }
        }
        mife_sk_free(sk);
        if (g_verbose)
            fprintf(stderr, "— Encryption time: %.2f s\n", current_time() - start);
    }
    return mife;
error:
    mife_free(mife);
    return NULL;
}

static mife_ct_t *
mife_encrypt(const mife_sk_t *sk, const size_t slot, const long *inputs,
             size_t ninputs, size_t nthreads, aes_randstate_t rng)
{
    mife_ct_t *ct = NULL;
    char *cmd = NULL, *inp = NULL, *seed_s = NULL;
    FILE *fp = NULL;
    long *seed = NULL;

    if (ninputs != acirc_symlen(sk->circ, slot)) {
        fprintf(stderr, "%s: %s: invalid input length for slot %lu (%lu ≠ %lu)\n",
                errorstr, __func__, slot, ninputs, acirc_symlen(sk->circ, slot));
        return NULL;
    }

    {
        const double start = current_time();
        if (g_verbose)
            fprintf(stderr, "— Generating input wire labels: ");
        inp = longs_to_str(inputs, acirc_symlen(sk->circ, slot));
        cmd = makestr("boots wires -d %s %s", sk->dirname, inp);
        if (system(cmd) != 0) {
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: error calling '%s'\n",
                    errorstr, __func__, cmd);
            goto cleanup;
        }
        if (g_verbose)
            fprintf(stderr, "%.2f s\n", current_time() - start);
    }
    {
        const double start = current_time();
        const size_t seedlen = sk->wirelen;
        char *fname = NULL;
        if (g_verbose)
            fprintf(stderr, "— Running MIFE encrypt on input seed\n");
        if ((seed_s = my_calloc(seedlen, sizeof seed_s[0])) == NULL)
            goto cleanup;
        if ((fname = makestr("%s/seed", sk->dirname)) == NULL)
            goto cleanup;
        if ((fp = fopen(fname, "r")) == NULL) {
            fprintf(stderr, "%s: %s: unable to open seed file '%s'\n",
                    errorstr, __func__, fname);
            free(fname);
            goto cleanup;
        }
        free(fname);
        if (fread(seed_s, sizeof seed_s[0], seedlen, fp) != seedlen) {
            fprintf(stderr, "%s: %s: unable to read seed\n",
                    errorstr, __func__);
            goto cleanup;
        }
        seed = str_to_longs(seed_s, seedlen);
        if ((ct = my_calloc(1, sizeof ct[0])) == NULL)
            goto cleanup;
        ct->vt = &mife_cmr_vtable;
        ct->ct = sk->vt->mife_encrypt(sk->sk, slot, seed, seedlen, nthreads, rng);
        ct->gc = sk->gc;
        if (g_verbose)
            fprintf(stderr, "— MIFE encrypt on input seed: %.2f s\n",
                    current_time() - start);
    }
cleanup:
    if (seed_s)
        free(seed_s);
    if (fp)
        fclose(fp);
    if (seed)
        free(seed);
    if (inp)
        free(inp);
    if (cmd)
        free(cmd);
    return ct;
}

static int
mife_decrypt(const mife_ek_t *ek, long *rop, const mife_ct_t **cts,
             size_t nthreads, size_t *kappa)
{
    const size_t noutputs = acirc_noutputs(ek->gc);
    mife_ct_t **cmr_cts = NULL;
    char *outs = NULL, *fname;
    FILE *fp = NULL;
    long *rop_ = NULL;
    int ret = ERR;
    bool save = true, load = false;

    if ((rop_ = my_calloc(acirc_noutputs(ek->gc), sizeof rop_[0])) == NULL)
        goto cleanup;
    if ((cmr_cts = my_calloc(acirc_nsymbols(ek->gc), sizeof cmr_cts[0])) == NULL)
        goto cleanup;
    for (size_t i = 0; i < acirc_nsymbols(ek->circ); ++i)
        cmr_cts[i] = cts[i]->ct;
    if ((fname = makestr("%s/gates", ek->dirname)) == NULL)
        goto cleanup;
    if ((fp = fopen(fname, "w")) == NULL) {
        fprintf(stderr, "%s: %s: unable to open gates file '%s'\n",
                errorstr, __func__, fname);
        free(fname);
        goto cleanup;
    }
    free(fname);
    /* decrypt MIFE to generate garbled circuit */
    for (size_t i = 0; i < acirc_symlen(ek->gc, 1); ++i) { /* XXX */
        const double start = current_time();
        mife_ct_t *ct = NULL;
        char *ctname = NULL, *wire = NULL;
        int ret_ = ERR;
        if (g_verbose)
            fprintf(stderr, "— Running MIFE decrypt on garbled gate #%lu: ", i);
        ctname = makestr("%s.ix%lu.1.ct", acirc_fname(ek->gc), i);
        if ((ct = mife_ct_read(ek->mmap, &mife_gc_vtable, ek, ctname)) == NULL) {
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: failed to read ciphertext\n",
                    errorstr, __func__);
            goto cleanup_;
        }
        cmr_cts[acirc_nsymbols(ek->circ)] = ct->ct;
        if (mife_cmr_decrypt(ek->ek, rop_, (const mife_ct_t **) cmr_cts,
                             nthreads, kappa, save, load) == ERR) {
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: mife decrypt failed on garbled circuit\n",
                    errorstr, __func__);
            goto cleanup_;
        }
        wire = longs_to_str(rop_, noutputs);
        if (fwrite(wire, sizeof wire[0], noutputs, fp) != noutputs
            || fwrite("\n", sizeof(char), 1, fp) != 1) {
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: failed to write wire to file\n",
                    errorstr, __func__);
            goto cleanup_;
        }
        ret_ = OK;
        acirc_set_saved(ek->gc);
        save = false;
        load = true;
    cleanup_:
        if (wire)
            free(wire);
        if (ctname)
            free(ctname);
        if (ct)
            mife_ct_free(ct);
        if (ret_ == ERR)
            goto cleanup;
        if (g_verbose)
            fprintf(stderr, "%.2f s\n", current_time() - start);
    }
    fclose(fp);
    fp = NULL;
    /* evaluate the garbled circuit */
    {
        const double start = current_time();
        char *cmd = NULL;
        if (g_verbose)
            fprintf(stderr, "— Evaluating the garbled circuit: ");
        if ((outs = my_calloc(1025, sizeof outs[0])) == NULL)
            goto cleanup;
        if ((cmd = makestr("boots eval -d %s", ek->dirname)) == NULL)
            goto cleanup;
        if ((fp = popen(cmd, "r")) == NULL) {
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: failed to run '%s'\n",
                    errorstr, __func__, cmd);
            free(cmd);
            goto cleanup;
        }
        free(cmd);
        if (fgets(outs, 1025, fp) == NULL) { /* XXX */
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: failed to get output\n",
                    errorstr, __func__);
            pclose(fp); fp = NULL;
            goto cleanup;
        }
        if (pclose(fp)) {
            if (g_verbose) fprintf(stderr, "\n");
            fprintf(stderr, "%s: %s: '%s' failed: %s\n",
                    errorstr, __func__, "boots eval", outs);
            fp = NULL;
            goto cleanup;
        }
        if (rop)
            for (size_t i = 0; i < acirc_noutputs(ek->circ); ++i)
                rop[i] = char_to_long(outs[i]);
        if (g_verbose)
            fprintf(stderr, "%.2f s\n", current_time() - start);
    }
    fp = NULL;
    ret = OK;
cleanup:
    if (outs)
        free(outs);
    if (fp)
        fclose(fp);
    if (cmr_cts)
        free(cmr_cts);
    if (rop_)
        free(rop_);
    return ret;
}

mife_vtable mife_gc_vtable = {
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
    .mife_setup = mife_setup,
    .mife_free = mife_free,
    .mife_encrypt = mife_encrypt,
    .mife_decrypt = mife_decrypt,
};
