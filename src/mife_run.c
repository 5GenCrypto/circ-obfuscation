#include "mife_run.h"
/* #include "mife_params.h" */
#include "util.h"

#include <string.h>
#include <mmap/mmap_dummy.h>

int
mife_run_setup(const mmap_vtable *mmap, const mife_vtable *vt,
               const char *circuit, const obf_params_t *op,
               size_t secparam, size_t *kappa, size_t npowers,
               size_t nthreads, aes_randstate_t rng)
{
    const double start = current_time();
    char skname[strlen(circuit) + sizeof ".sk\0"];
    char ekname[strlen(circuit) + sizeof ".ek\0"];
    mife_t *mife = NULL;
    mife_sk_t *sk = NULL;
    mife_ek_t *ek = NULL;
    FILE *fp = NULL;
    int ret = ERR;

    if (g_verbose) {
        fprintf(stderr, "MIFE setup details:\n");
        fprintf(stderr, "* circuit: ............. %s\n", circuit);
        fprintf(stderr, "* security parameter: .. %lu\n", secparam);
        fprintf(stderr, "* # threads: ........... %lu\n", nthreads);
        /* fprintf(stderr, "* # encodings: ......... %lu\n", mife_num_encodings_setup(cp, npowers)); */
    }

    if ((mife = vt->mife_setup(mmap, op, secparam, kappa, npowers, nthreads, rng)) == NULL)
        goto cleanup;
    {
        const double _start = current_time();
        if ((sk = vt->mife_sk(mife)) == NULL)
            goto cleanup;
        snprintf(skname, sizeof skname, "%s.sk", circuit);
        if ((fp = fopen(skname, "w")) == NULL) {
            fprintf(stderr, "%s: %s: unable to open '%s' for writing\n",
                    errorstr, __func__, skname);
            goto cleanup;
        }
        if (vt->mife_sk_fwrite(sk, fp) == ERR)
            goto cleanup;
        fclose(fp);
        if (g_verbose) {
            fprintf(stderr, "  Writing secret key to disk: %.2fs\n",
                    current_time() - _start);
            fprintf(stderr, "    Secret key file size: %lu KB\n",
                    filesize(skname) / 1024);
        }
    }
    {
        const double _start = current_time();
        if ((ek = vt->mife_ek(mife)) == NULL)
            goto cleanup;
        snprintf(ekname, sizeof ekname, "%s.ek", circuit);
        if ((fp = fopen(ekname, "w")) == NULL) {
            fprintf(stderr, "%s: %s: unable to open '%s' for writing\n",
                    errorstr, __func__, ekname);
            goto cleanup;
        }
        if (vt->mife_ek_fwrite(ek, fp) == ERR)
            goto cleanup;
        fclose(fp);
        if (g_verbose) {
            fprintf(stderr, "  Writing evaluation key to disk: %.2fs\n",
                    current_time() - _start);
            fprintf(stderr, "    Evaluation key file size: %lu KB\n",
                    filesize(ekname) / 1024);
        }
    }
    if (g_verbose)
        fprintf(stderr, "MIFE setup time: %.2fs\n", current_time() - start);
    fp = NULL;
    ret = OK;
cleanup:
    if (fp)
        fclose(fp);
    if (sk)
        vt->mife_sk_free(sk);
    if (ek)
        vt->mife_ek_free(ek);
    if (mife)
        vt->mife_free(mife);
    return ret;
}

int
mife_run_encrypt(const mmap_vtable *mmap, const mife_vtable *vt,
                 const char *circuit, const obf_params_t *op, const long *input,
                 size_t slot, size_t nthreads, mife_sk_t *cached_sk,
                 aes_randstate_t rng)
{
    const double start = current_time();
    const circ_params_t *cp = obf_params_cp(op);
    mife_ct_t *ct = NULL;
    mife_sk_t *sk = NULL;
    size_t ninputs;
    FILE *fp = NULL;
    int ret = ERR;

    if (slot >= cp->nslots) {
        fprintf(stderr, "error: invalid MIFE slot %lu\n", slot);
        return ERR;
    }
    ninputs = cp->ds[slot];

    if (g_verbose) {
        fprintf(stderr, "MIFE encryption details:\n");
        fprintf(stderr, "* circuit: ....... %s\n", circuit);
        fprintf(stderr, "* slot: .......... %lu\n", slot);
        fprintf(stderr, "* input: ......... ");
        for (size_t i = 0; i < ninputs; ++i)
            fprintf(stderr, "%ld", input[i]);
        fprintf(stderr, "\n");
        fprintf(stderr, "* # threads: ..... %lu\n", nthreads);
        /* XXX */
        /* fprintf(stderr, "* # encodings: ... %lu\n", */
        /*         mife_num_encodings_encrypt(cp, slot)); */
    }

    if (cached_sk == NULL) {
        const double _start = current_time();
        char skname[strlen(circuit) + sizeof ".sk\0"];

        snprintf(skname, sizeof skname, "%s.sk", circuit);
        if ((fp = fopen(skname, "r")) == NULL) {
            fprintf(stderr, "error: %s: unable to open '%s' for reading\n",
                    __func__, skname);
            goto cleanup;
        }
        if ((sk = vt->mife_sk_fread(mmap, op, fp)) == NULL) {
            fprintf(stderr, "error: %s: unable to read secret key from disk\n",
                    __func__);
            goto cleanup;
        }
        fclose(fp);
        if (g_verbose)
            fprintf(stderr, "  Reading secret key from disk: %.2fs\n",
                    current_time() - _start);
    } else {
        sk = cached_sk;
    }

    if ((ct = vt->mife_encrypt(sk, slot, input, nthreads, rng)) == NULL) {
        fprintf(stderr, "error: %s: encryption failed\n", __func__);
        goto cleanup;
    }

    {
        double _start = current_time();
        char ctname[strlen(circuit) + 10 + strlen("..ct\0")];
        snprintf(ctname, sizeof ctname, "%s.%lu.ct", circuit, slot);

        if ((fp = fopen(ctname, "w")) == NULL) {
            fprintf(stderr, "error: %s: unable to open '%s' for writing\n",
                    __func__, ctname);
            goto cleanup;
        }
        if (vt->mife_ct_fwrite(ct, cp, fp) == ERR) {
            fprintf(stderr, "error: %s: unable to write ciphertext to disk\n",
                    __func__);
            goto cleanup;
        }
        fclose(fp);
        if (g_verbose) {
            fprintf(stderr, "  Writing ciphertext to disk: %.2fs\n", current_time() - _start);
            fprintf(stderr, "    Ciphertext file size: %lu KB\n", filesize(ctname) / 1024);
        }
    }
    if (g_verbose)
        fprintf(stderr, "MIFE encryption time: %.2fs\n", current_time() - start);
    fp = NULL;
    ret = OK;
cleanup:
    if (fp)
        fclose(fp);
    if (ct)
        vt->mife_ct_free(ct, cp);
    if (cached_sk == NULL && sk)
        vt->mife_sk_free(sk);
    return ret;
}

int
mife_run_decrypt(const mmap_vtable *mmap, const mife_vtable *vt,
                 const char *ek_s, char **cts_s, long *rop,
                 const obf_params_t *op, size_t *kappa, size_t nthreads)
{
    const double start = current_time();
    const circ_params_t *cp = obf_params_cp(op);
    const size_t has_consts = acirc_nconsts(cp->circ) + acirc_nsecrets(cp->circ) ? 1 : 0;
    mife_ct_t *cts[cp->nslots];
    mife_ek_t *ek = NULL;
    FILE *fp;
    int ret = ERR;

    memset(cts, '\0', sizeof cts);

    if (g_verbose) {
        fprintf(stderr, "MIFE decryption details:\n");
        fprintf(stderr, "* evaluation key: . %s\n", ek_s);
        fprintf(stderr, "* ciphertexts: .... ");
        for (size_t i = 0; i < cp->nslots - has_consts; ++i) {
            fprintf(stderr, "%s%s\n", i == 0 ? "" : "                    ", cts_s[i]);
        }
    }

    {
        const double _start = current_time();
        if ((fp = fopen(ek_s, "r")) == NULL) {
            fprintf(stderr, "error: %s: unable to open '%s' for reading\n",
                    __func__, ek_s);
            goto cleanup;
        }
        if ((ek = vt->mife_ek_fread(mmap, op, fp)) == NULL) {
            fprintf(stderr, "error: %s: unable to read evaluation key\n",
                    __func__);
            goto cleanup;
        }
        fclose(fp);
        if (g_verbose)
            fprintf(stderr, "  Reading evaluation key from disk: %.2fs\n",
                    current_time() - _start);
    }

    for (size_t i = 0; i < cp->nslots - has_consts; ++i) {
        const double _start = current_time();
        if ((fp = fopen(cts_s[i], "r")) == NULL) {
            fprintf(stderr, "error: %s: unable to open '%s' for reading\n",
                    __func__, cts_s[i]);
            goto cleanup;
        }
        if ((cts[i] = vt->mife_ct_fread(mmap, cp, fp)) == NULL) {
            fprintf(stderr, "error: %s: unable to read ciphertext for slot %lu\n",
                    __func__, i);
            goto cleanup;
        }
        fclose(fp);
        if (g_verbose)
            fprintf(stderr, "  Reading ciphertext #%lu from disk: %.2fs\n",
                    i, current_time() - _start);
    }
    if (vt->mife_decrypt(ek, rop, (const mife_ct_t **) cts, nthreads, kappa) == ERR) {
        fprintf(stderr, "error: %s: decryption failed\n", __func__);
        goto cleanup;
    }
    if (g_verbose)
        fprintf(stderr, "MIFE decryption time: %.2fs\n", current_time() - start);
    fp = NULL;
    ret = OK;
cleanup:
    if (fp)
        fclose(fp);
    if (ek)
        vt->mife_ek_free(ek);
    for (size_t i = 0; i < cp->nslots; ++i) {
        if (cts[i])
            vt->mife_ct_free(cts[i], cp);
    }
    return ret;
}

static int
mife_run_all(const mmap_vtable *mmap, const mife_vtable *vt,
             const char *circuit, const obf_params_t *op, long **inp,
             long *outp, size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    const circ_params_t *cp = obf_params_cp(op);
    const acirc_t *const circ = cp->circ;
    mife_sk_t *sk = NULL;
    size_t consts = acirc_nconsts(circ) + acirc_nsecrets(cp->circ) ? 1 : 0;
    int ret = ERR;

    {
        const double _start = current_time();
        char skname[strlen(circuit) + sizeof ".sk\0"];
        FILE *fp;

        snprintf(skname, sizeof skname, "%s.sk", circuit);
        if ((fp = fopen(skname, "r")) == NULL) {
            fprintf(stderr, "error: %s: unable to open '%s' for reading\n",
                    __func__, skname);
            return ERR;
        }
        sk = vt->mife_sk_fread(mmap, op, fp);
        fclose(fp);
        if (sk == NULL)
            return ERR;
        if (g_verbose)
            fprintf(stderr, "  Reading secret key from disk: %.2fs\n",
                    current_time() - _start);
    }

    /* Encrypt each input in the right slot */
    for (size_t i = 0; i < cp->nslots - consts; ++i) {
        if (mife_run_encrypt(mmap, vt, circuit, op, inp[i], i, nthreads, sk, rng) == ERR) {
            fprintf(stderr, "error: %s: mife encryption of '", __func__);
            for (size_t j = 0; j < cp->ds[i]; ++j) {
                fprintf(stderr, "%ld", inp[i][j]);
            }
            fprintf(stderr, "' in slot %lu failed\n", i);
            return ERR;
        }
    }

    vt->mife_sk_free(sk);

    {
        char ek[strlen(circuit) + sizeof ".ek\0"];
        char *cts[cp->nslots];

        snprintf(ek, sizeof ek, "%s.ek", circuit);
        for (size_t j = 0; j < cp->nslots - consts; ++j) {
            size_t length = strlen(circuit) + 10 + sizeof ".ct\0";
            cts[j] = my_calloc(length, sizeof cts[j][0]);
            snprintf(cts[j], length, "%s.%lu.ct", circuit, j);
        }
        ret = mife_run_decrypt(mmap, vt, ek, cts, outp, op, kappa, nthreads);
        for (size_t j = 0; j < cp->nslots - consts; ++j) {
            free(cts[j]);
        }
    }
    return ret;
}

int
mife_run_test(const mmap_vtable *mmap, const mife_vtable *vt,
              const char *circuit, obf_params_t *op, size_t secparam,
              size_t *kappa, size_t npowers, size_t nthreads,
              aes_randstate_t rng)
{
    const circ_params_t *cp = obf_params_cp(op);
    const acirc_t *const circ = cp->circ;
    const size_t has_consts = acirc_nconsts(circ) + acirc_nsecrets(cp->circ) ? 1 : 0;
    int ret = OK;

    if (mife_run_setup(mmap, vt, circuit, op, secparam, kappa, npowers, nthreads, rng) == ERR)
        return ERR;

    for (size_t t = 0; t < acirc_ntests(circ); ++t) {
        long *inps[acirc_nsymbols(circ)];
        long outp[acirc_noutputs(circ)];
        size_t idx = 0;
        for (size_t i = 0; i < cp->nslots - has_consts; ++i) {
            inps[i] = my_calloc(cp->ds[i], sizeof inps[i][0]);
            memcpy(inps[i], &acirc_test_input(circ, t)[idx], cp->ds[i] * sizeof inps[i][0]);
            idx += cp->ds[i];
        }
        if (mife_run_all(mmap, vt, circuit, op, inps, outp, kappa, nthreads, rng) == ERR)
            return ERR;
        if (!print_test_output(t + 1, acirc_test_input(circ, t), acirc_ninputs(circ),
                               acirc_test_output(circ, t), outp, acirc_noutputs(circ), false))
            ret = ERR;
        for (size_t i = 0; i < cp->nslots - has_consts; ++i) {
            free(inps[i]);
        }
    }
    if (g_verbose) {
        unsigned long size, resident;
        if (memory(&size, &resident) == OK)
            fprintf(stderr, "memory: %lu MB\n", resident);
    }
    return ret;
}

size_t
mife_run_smart_kappa(const mife_vtable *vt, const char *circuit,
                     const obf_params_t *op, size_t npowers, size_t nthreads,
                     aes_randstate_t rng)
{
    const circ_params_t *cp = obf_params_cp(op);
    const size_t has_consts = acirc_nconsts(cp->circ) + acirc_nsecrets(cp->circ) ? 1 : 0;
    long *inps[cp->nslots - has_consts];
    size_t kappa = 1;
    bool verbosity = g_verbose;

    if (g_verbose)
        fprintf(stderr, "Choosing κ smartly... ");

    g_verbose = false;
    if (mife_run_setup(&dummy_vtable, vt, circuit, op, 8, &kappa, npowers, nthreads, rng) == ERR)
        goto cleanup;

    for (size_t i = 0; i < cp->nslots - has_consts; ++i)
        inps[i] = my_calloc(cp->ds[i], sizeof inps[i][0]);
    if (mife_run_all(&dummy_vtable, vt, circuit, op, inps, NULL, &kappa, nthreads, rng) == ERR) {
        fprintf(stderr, "error: %s: unable to determine κ smartly\n", __func__);
        kappa = 0;
    }
    for (size_t i = 0; i < cp->nslots - has_consts; ++i)
        free(inps[i]);
cleanup:
    g_verbose = verbosity;
    if (g_verbose)
        fprintf(stderr, "%lu\n", kappa);
    return kappa;
}
