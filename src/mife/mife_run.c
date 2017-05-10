#include "mife_run.h"
#include "mife_params.h"
#include "util.h"

#include <string.h>
#include <mmap/mmap_dummy.h>

int
mife_run_setup(const mmap_vtable *mmap, const char *circuit, obf_params_t *op,
               size_t secparam, size_t *kappa, size_t npowers, size_t nthreads,
               aes_randstate_t rng)
{
    const circ_params_t *cp = &op->cp;
    char skname[strlen(circuit) + sizeof ".sk\0"];
    char ekname[strlen(circuit) + sizeof ".ek\0"];
    mife_t *mife = NULL;
    mife_sk_t *sk = NULL;
    mife_ek_t *ek = NULL;
    FILE *fp;
    int ret = ERR;

    if (g_verbose) {
        fprintf(stderr, "MIFE setup details:\n");
        fprintf(stderr, "* circuit: ............. %s\n", circuit);
        fprintf(stderr, "* security parameter: .. %lu\n", secparam);
        fprintf(stderr, "* # threads: ........... %lu\n", nthreads);
        fprintf(stderr, "* # encodings: ......... %lu\n", mife_num_encodings_setup(cp, npowers));
        circ_params_print(cp);
    }

    mife = mife_setup(mmap, op, secparam, kappa, npowers, nthreads, rng);
    if (mife == NULL)
        goto cleanup;
    sk = mife_sk(mife);
    snprintf(skname, sizeof skname, "%s.sk", circuit);
    if ((fp = fopen(skname, "w")) == NULL) {
        fprintf(stderr, "error: unable to open '%s' for writing\n", skname);
        goto cleanup;
    }
    mife_sk_fwrite(sk, fp);
    fclose(fp);

    ek = mife_ek(mife);
    snprintf(ekname, sizeof ekname, "%s.ek", circuit);
    if ((fp = fopen(ekname, "w")) == NULL) {
        fprintf(stderr, "error: unable to open '%s' for writing\n", ekname);
        goto cleanup;
    }
    mife_ek_fwrite(ek, fp);
    fclose(fp);

    ret = OK;
cleanup:
    mife_sk_free(sk);
    mife_ek_free(ek);
    mife_free(mife);

    return ret;
}

int
mife_run_encrypt(const mmap_vtable *mmap, const char *circuit, obf_params_t *op,
                 const int *input, size_t slot, size_t nthreads,
                 mife_sk_t *cached_sk, aes_randstate_t rng)
{
    const circ_params_t *cp = &op->cp;
    double start, end;
    mife_ciphertext_t *ct;
    mife_sk_t *sk;
    size_t ninputs;
    FILE *fp;

    if (slot >= cp->n) {
        fprintf(stderr, "error: invalid MIFE slot %lu\n", slot);
        exit(EXIT_FAILURE);
    }
    ninputs = cp->ds[slot];

    if (g_verbose) {
        fprintf(stderr, "MIFE encryption details:\n");
        fprintf(stderr, "* circuit: ....... %s\n", circuit);
        fprintf(stderr, "* slot: .......... %lu\n", slot);
        fprintf(stderr, "* input: ......... ");
        for (size_t i = 0; i < ninputs; ++i)
            fprintf(stderr, "%d", input[i]);
        fprintf(stderr, "\n");
        fprintf(stderr, "* # threads: ..... %lu\n", nthreads);
        fprintf(stderr, "* # encodings: ... %lu\n",
                mife_num_encodings_encrypt(cp, slot));
    }

    if (cached_sk == NULL) {
        char skname[strlen(circuit) + sizeof ".sk\0"];

        start = current_time();
        snprintf(skname, sizeof skname, "%s.sk", circuit);
        if ((fp = fopen(skname, "r")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for reading\n", skname);
            exit(EXIT_FAILURE);
        }
        sk = mife_sk_fread(mmap, op, fp);
        fclose(fp);

        end = current_time();
        if (g_verbose)
            fprintf(stderr, "  Reading sk from disk: %.2fs\n", end - start);
    } else {
        sk = cached_sk;
    }

    ct = mife_encrypt(sk, slot, input, nthreads, NULL, rng);
    if (ct == NULL) {
        fprintf(stderr, "error: encryption failed\n");
        exit(EXIT_FAILURE);
    }

    start = current_time();
    {
        char ctname[strlen(circuit) + 10 + strlen("..ct\0")];
        snprintf(ctname, sizeof ctname, "%s.%lu.ct", circuit, slot);

        if ((fp = fopen(ctname, "w")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for writing\n", ctname);
            exit(EXIT_FAILURE);
        }
        mife_ciphertext_fwrite(ct, cp, fp);
        fclose(fp);
    }
    end = current_time();
    if (g_verbose)
        fprintf(stderr, "  Writing ct to disk: %.2fs\n", end - start);
    mife_ciphertext_free(ct, cp);
    if (cached_sk == NULL)
        mife_sk_free(sk);

    return OK;
}

int
mife_run_decrypt(const char *ek_s, char **cts_s, int *rop,
                 const mmap_vtable *mmap, obf_params_t *op, size_t *kappa,
                 size_t nthreads)
{
    const circ_params_t *cp = &op->cp;
    const size_t has_consts = cp->circ->consts.n ? 1 : 0;
    mife_ciphertext_t *cts[cp->n];
    mife_ek_t *ek = NULL;
    FILE *fp;
    int ret = ERR;

    memset(cts, '\0', sizeof cts);

    if (g_verbose) {
        fprintf(stderr, "MIFE decryption details:\n");
        fprintf(stderr, "* evaluation key: . %s\n", ek_s);
        fprintf(stderr, "* ciphertexts: .... ");
        for (size_t i = 0; i < cp->n - has_consts; ++i) {
            fprintf(stderr, "%s%s\n", i == 0 ? "" : "                    ", cts_s[i]);
        }
    }

    if ((fp = fopen(ek_s, "r")) == NULL) {
        fprintf(stderr, "error: unable to open '%s' for reading\n", ek_s);
        goto cleanup;
    }
    ek = mife_ek_fread(mmap, op, fp);
    fclose(fp);
    if (ek == NULL) {
        fprintf(stderr, "error: unable to read evaluation key\n");
        goto cleanup;
    }
    for (size_t i = 0; i < cp->n - has_consts; ++i) {
        if ((fp = fopen(cts_s[i], "r")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for reading\n", cts_s[i]);
            goto cleanup;
        }
        cts[i] = mife_ciphertext_fread(mmap, cp, fp);
        fclose(fp);
        if (cts[i] == NULL) {
            fprintf(stderr, "error: unable to read ciphertext for slot %lu\n", i);
            goto cleanup;
        }
    }
    ret = mife_decrypt(ek, rop, cts, nthreads, kappa);
    if (ret == ERR) {
        fprintf(stderr, "error: decryption failed\n");
        goto cleanup;
    }
    ret = OK;
cleanup:
    if (ek)
        mife_ek_free(ek);
    for (size_t i = 0; i < cp->n; ++i) {
        if (cts[i])
            mife_ciphertext_free(cts[i], cp);
    }
    return ret;
}

static int
mife_run_all(const mmap_vtable *mmap, const char *circuit, obf_params_t *op,
             int **inp, int *outp, size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    const circ_params_t *cp = &op->cp;
    const acirc *const circ = cp->circ;
    double start, end;
    mife_sk_t *sk;
    size_t consts = circ->consts.n ? 1 : 0;
    int ret = ERR;

    {
        char skname[strlen(circuit) + sizeof ".sk\0"];
        FILE *fp;

        start = current_time();
        snprintf(skname, sizeof skname, "%s.sk", circuit);
        if ((fp = fopen(skname, "r")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for reading\n", skname);
            exit(EXIT_FAILURE);
        }
        sk = mife_sk_fread(mmap, op, fp);
        fclose(fp);

        end = current_time();
        if (g_verbose)
            fprintf(stderr, "  Reading sk from disk: %.2fs\n", end - start);
    }

    /* Encrypt each input in the right slot */
    for (size_t i = 0; i < cp->n - consts; ++i) {
        ret = mife_run_encrypt(mmap, circuit, op, inp[i], i, nthreads, sk, rng);
        if (ret == ERR) {
            fprintf(stderr, "error: mife encryption of '");
            for (size_t j = 0; j < cp->ds[i]; ++j) {
                fprintf(stderr, "%d", inp[i][j]);
            }
            fprintf(stderr, "' in slot %lu failed\n", i);
            return ERR;
        }
    }
    mife_sk_free(sk);

    {
        char ek[strlen(circuit) + sizeof ".ek\0"];
        char *cts[cp->n];

        snprintf(ek, sizeof ek, "%s.ek", circuit);
        for (size_t j = 0; j < cp->n - consts; ++j) {
            size_t length = strlen(circuit) + 10 + sizeof ".ct\0";
            cts[j] = my_calloc(length, sizeof cts[j][0]);
            snprintf(cts[j], length, "%s.%lu.ct", circuit, j);
        }
        ret = mife_run_decrypt(ek, cts, outp, mmap, op, kappa, nthreads);
        for (size_t j = 0; j < cp->n - consts; ++j) {
            free(cts[j]);
        }
    }
    if (ret == ERR) {
        fprintf(stderr, "error: mife decryption failed\n");
        return ERR;
    }

    return OK;
}

int
mife_run_test(const mmap_vtable *mmap, const char *circuit, obf_params_t *op,
              size_t secparam, size_t *kappa, size_t npowers, size_t nthreads,
              aes_randstate_t rng)
{
    const circ_params_t *cp = &op->cp;
    const acirc *const circ = cp->circ;
    const size_t has_consts = circ->consts.n ? 1 : 0;
    int ret;

    ret = mife_run_setup(mmap, circuit, op, secparam, kappa, npowers, nthreads, rng);
    if (ret == ERR) {
        fprintf(stderr, "error: mife setup failed\n");
        return ERR;
    }

    for (size_t t = 0; t < circ->tests.n; ++t) {
        int *inps[cp->n - has_consts];
        int outp[cp->m];
        size_t idx = 0;
        for (size_t i = 0; i < cp->n - has_consts; ++i) {
            inps[i] = my_calloc(cp->ds[i], sizeof inps[i][0]);
            memcpy(inps[i], &circ->tests.inps[t][idx], cp->ds[i] * sizeof inps[i][0]);
            idx += cp->ds[i];
        }
        mife_run_all(mmap, circuit, op, inps, outp, kappa, nthreads, rng);
        if (!print_test_output(t + 1, circ->tests.inps[t], circ->ninputs,
                               circ->tests.outs[t], outp, circ->outputs.n, false))
            ret = ERR;
        for (size_t i = 0; i < cp->n - has_consts; ++i) {
            free(inps[i]);
        }
    }
    return ret;
}

size_t
mife_run_smart_kappa(const char *circuit, obf_params_t *op, size_t npowers,
                     size_t nthreads, aes_randstate_t rng)
{
    const circ_params_t *cp = &op->cp;
    const size_t has_consts = cp->circ->consts.n ? 1 : 0;
    int *inps[cp->n - has_consts];
    size_t kappa = 0;
    bool verbosity = g_verbose;

    if (g_verbose)
        fprintf(stderr, "Choosing κ smartly...\n");

    g_verbose = false;
    if (mife_run_setup(&dummy_vtable, circuit, op, 8, &kappa, npowers, nthreads, rng) == ERR) {
        fprintf(stderr, "error: mife setup failed\n");
        goto cleanup;
    }

    for (size_t i = 0; i < cp->n - has_consts; ++i) {
        inps[i] = my_calloc(cp->ds[i], sizeof inps[i][0]);
    }
    if (mife_run_all(&dummy_vtable, circuit, op, inps, NULL, &kappa, nthreads, rng) == ERR) {
        fprintf(stderr, "error: unable to determine κ smartly\n");
        kappa = 0;
    }
    for (size_t i = 0; i < cp->n - has_consts; ++i) {
        free(inps[i]);
    }
cleanup:
    g_verbose = verbosity;
    return kappa;
}
