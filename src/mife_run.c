#include "mife_run.h"
#include "mife_util.h"
#include "util.h"

#include <string.h>
#include <mmap/mmap_dummy.h>

int
mife_run_setup(const mmap_vtable *mmap, const mife_vtable *vt,
               const acirc_t *circ, const obf_params_t *op,
               size_t secparam, const char *ekname_, const char *skname_,
               size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    const double start = current_time();
    const char *circuit = acirc_fname(circ);
    char *skname = NULL, *ekname = NULL;
    mife_t *mife = NULL;
    mife_sk_t *sk = NULL;
    mife_ek_t *ek = NULL;
    FILE *fp = NULL;
    int ret = ERR;

    if (ekname_ == NULL)
        ekname = makestr("%s.ek", circuit);
    else
        ekname = (char *) ekname_;
    if (skname_ == NULL)
        skname = makestr("%s.sk", circuit);
    else
        skname = (char *) skname_;

    if (g_verbose) {
        fprintf(stderr, "MIFE setup details:\n");
        fprintf(stderr, "———— circuit: ............. %s\n", circuit);
        fprintf(stderr, "———— ek: .................. %s\n", ekname);
        fprintf(stderr, "———— sk: .................. %s\n", skname);
        fprintf(stderr, "———— security parameter: .. %lu\n", secparam);
        fprintf(stderr, "———— # threads: ........... %lu\n", nthreads);
    }

    if ((mife = vt->mife_setup(mmap, circ, op, secparam, kappa, nthreads, rng)) == NULL)
        goto cleanup;
    {
        const double _start = current_time();
        if ((sk = vt->mife_sk(mife)) == NULL)
            goto cleanup;
        if ((fp = fopen(skname, "w")) == NULL) {
            fprintf(stderr, "%s: unable to open '%s' for writing\n",
                    errorstr, skname);
            goto cleanup;
        }
        if (vt->mife_sk_fwrite(sk, fp) == ERR)
            goto cleanup;
        fclose(fp);
        if (g_verbose)
            fprintf(stderr, "— Writing secret key to disk: %.2f s [%lu KB]\n",
                    current_time() - _start, filesize(skname) / 1024);
    }
    fp = NULL;
    {
        const double _start = current_time();
        if ((ek = vt->mife_ek(mife)) == NULL)
            goto cleanup;
        if ((fp = fopen(ekname, "w")) == NULL) {
            fprintf(stderr, "%s: unable to open '%s' for writing\n",
                    errorstr, ekname);
            goto cleanup;
        }
        if (vt->mife_ek_fwrite(ek, fp) == ERR)
            goto cleanup;
        fclose(fp);
        if (g_verbose)
            fprintf(stderr, "— Writing evaluation key to disk: %.2f s [%lu KB]\n",
                    current_time() - _start, filesize(ekname) / 1024);
    }
    fp = NULL;
    if (g_verbose)
        fprintf(stderr, "MIFE setup time: %.2f s\n", current_time() - start);
    ret = OK;
cleanup:
    if (ekname_ == NULL && ekname)
        free(ekname);
    if (skname_ == NULL && skname)
        free(skname);
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
                 const acirc_t *circ, const long *input, size_t ninputs,
                 size_t slot, const char *skname_, size_t nthreads,
                 aes_randstate_t rng)
{
    const double start = current_time();
    const char *circuit = acirc_fname(circ);
    char *skname = NULL;
    mife_ct_t *ct = NULL;
    mife_sk_t *sk = NULL;
    FILE *fp = NULL;
    int ret = ERR;

    if (slot >= acirc_nsymbols(circ)) {
        fprintf(stderr, "%s: invalid MIFE slot %lu\n", errorstr, slot);
        return ERR;
    }
    if (ninputs != acirc_symlen(circ, slot)) {
        fprintf(stderr, "%s: invalid number of inputs for given slot\n",
                errorstr);
        return ERR;
    }

    if (skname_ == NULL) {
        skname = makestr("%s.sk", circuit);
    } else {
        skname = (char *) skname_;
    }

    if (g_verbose) {
        fprintf(stderr, "MIFE encryption details:\n");
        fprintf(stderr, "———— circuit: ....... %s\n", circuit);
        fprintf(stderr, "———— sk: ............ %s\n", skname);
        fprintf(stderr, "———— slot: .......... %lu\n", slot);
        fprintf(stderr, "———— input: ......... ");
        for (size_t i = 0; i < ninputs; ++i)
            fprintf(stderr, "%ld", input[i]);
        fprintf(stderr, "\n");
        fprintf(stderr, "———— # threads: ..... %lu\n", nthreads);
    }

    {
        const double _start = current_time();

        if ((fp = fopen(skname, "r")) == NULL) {
            fprintf(stderr, "%s: unable to open '%s' for reading\n",
                    errorstr, skname);
            goto cleanup;
        }
        if ((sk = vt->mife_sk_fread(mmap, circ, fp)) == NULL) {
            fprintf(stderr, "%s: unable to read secret key from disk\n",
                    errorstr);
            goto cleanup;
        }
        fclose(fp);
        fp = NULL;
        if (g_verbose)
            fprintf(stderr, "— Reading secret key from disk: %.2f s [%lu KB]\n",
                    current_time() - _start, filesize(skname) / 1024);
        if (skname_ == NULL)
            free(skname);
    }

    if ((ct = vt->mife_encrypt(sk, slot, input, ninputs, nthreads, rng)) == NULL) {
        fprintf(stderr, "%s: MIFE encryption failed\n", errorstr);
        goto cleanup;
    }

    {
        double _start = current_time();
        char *ctname;

        if ((ctname = makestr("%s.%lu.ct", circuit, slot)) == NULL)
            goto cleanup;
        if (mife_ct_write(vt, ct, ctname) == ERR) {
            free(ctname);
            goto cleanup;
        }
        if (g_verbose)
            fprintf(stderr, "— Writing ciphertext to disk: %.2f s [%lu KB]\n",
                    current_time() - _start, filesize(ctname) / 1024);
        free(ctname);
    }
    if (g_verbose)
        fprintf(stderr, "MIFE encryption time: %.2f s\n", current_time() - start);
    ret = OK;
cleanup:
    if (fp)
        fclose(fp);
    if (ct)
        vt->mife_ct_free(ct);
    if (sk)
        vt->mife_sk_free(sk);
    return ret;
}

int
mife_run_decrypt(const mmap_vtable *mmap, const mife_vtable *vt,
                 const acirc_t *circ, const char *ek_s, char **cts_s, long *rop,
                 size_t *kappa, size_t nthreads)
{
    const double start = current_time();
    mife_ct_t *cts[acirc_nsymbols(circ)];
    mife_ek_t *ek = NULL;
    int ret = ERR;

    memset(cts, '\0', sizeof cts);

    if (g_verbose) {
        fprintf(stderr, "MIFE decryption details:\n");
        fprintf(stderr, "———— evaluation key: . %s\n", ek_s);
        fprintf(stderr, "———— ciphertexts: .... ");
        for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
            fprintf(stderr, "%s%s\n", i == 0 ? "" : "                       ", cts_s[i]);
        }
    }

    {
        const double _start = current_time();
        FILE *fp;
        if ((fp = fopen(ek_s, "r")) == NULL) {
            fprintf(stderr, "%s: unable to open '%s' for reading\n",
                    errorstr, ek_s);
            goto cleanup;
        }
        if ((ek = vt->mife_ek_fread(mmap, circ, fp)) == NULL) {
            fprintf(stderr, "%s: unable to read evaluation key\n",
                    errorstr);
            fclose(fp);
            goto cleanup;
        }
        fclose(fp);
        if (g_verbose)
            fprintf(stderr, "— Reading evaluation key from disk: %.2f s [%lu KB]\n",
                    current_time() - _start, filesize(ek_s) / 1024);
    }

    for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
        const double _start = current_time();
        if ((cts[i] = mife_ct_read(mmap, vt, ek, cts_s[i])) == NULL)
            goto cleanup;
        if (g_verbose)
            fprintf(stderr, "— Reading ciphertext #%lu from disk: %.2f s [%lu KB]\n",
                    i, current_time() - _start, filesize(cts_s[i]) / 1024);
    }
    if (vt->mife_decrypt(ek, rop, (const mife_ct_t **) cts, nthreads, kappa) == ERR) {
        fprintf(stderr, "%s: MIFE decryption failed\n", errorstr);
        goto cleanup;
    }
    if (g_verbose)
        fprintf(stderr, "MIFE decryption time: %.2f s\n", current_time() - start);
    ret = OK;
cleanup:
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        if (cts[i])
            vt->mife_ct_free(cts[i]);
    if (ek)
        vt->mife_ek_free(ek);
    return ret;
}

static int
mife_run_all(const mmap_vtable *mmap, const mife_vtable *vt,
             const acirc_t *circ, long **inp, long *outp,
             size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    const char *circuit = acirc_fname(circ);
    char *skname = NULL;
    int ret = ERR;

    if ((skname = makestr("%s.sk", circuit)) == NULL)
        goto cleanup;

    /* Encrypt each input in the right slot */
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
        if (mife_run_encrypt(mmap, vt, circ, inp[i], acirc_symlen(circ, i),
                             i, skname, nthreads, rng) == ERR) {
            fprintf(stderr, "%s: mife encryption of '", errorstr);
            for (size_t j = 0; j < acirc_symlen(circ, i); ++j)
                fprintf(stderr, "%ld", inp[i][j]);
            fprintf(stderr, "' in slot %lu failed\n", i);
            goto cleanup;
        }
    }
    {
        char *ek = NULL;
        char *cts[acirc_nsymbols(circ)];

        if ((ek = makestr("%s.ek", circuit)) == NULL)
            goto cleanup;
        for (size_t j = 0; j < acirc_nsymbols(circ); ++j)
            cts[j] = makestr("%s.%lu.ct", circuit, j);
        ret = mife_run_decrypt(mmap, vt, circ, ek, cts, outp, kappa, nthreads);
        for (size_t j = 0; j < acirc_nsymbols(circ); ++j)
            free(cts[j]);
        free(ek);
    }
cleanup:
    if (skname)
        free(skname);
    return ret;
}

int
mife_run_test(const mmap_vtable *mmap, const mife_vtable *vt,
              const acirc_t *circ, obf_params_t *op, size_t secparam,
              size_t *kappa, size_t nthreads, aes_randstate_t rng)
{
    int ret = OK;

    if (mife_run_setup(mmap, vt, circ, op, secparam, NULL, NULL, kappa,
                       nthreads, rng) == ERR)
        return ERR;

    for (size_t t = 0; t < acirc_ntests(circ); ++t) {
        long *inps[acirc_nsymbols(circ)];
        long outp[acirc_noutputs(circ)];
        size_t idx = 0;
        for (size_t i = 0; i < acirc_nsymbols(circ); ++i) {
            inps[i] = xcalloc(acirc_symlen(circ, i), sizeof inps[i][0]);
            memcpy(inps[i], &acirc_test_input(circ, t)[idx],
                   acirc_symlen(circ, i) * sizeof inps[i][0]);
            idx += acirc_symlen(circ, i);
        }
        if (mife_run_all(mmap, vt, circ, inps, outp, kappa, nthreads,
                         rng) == OK) {
            if (!print_test_output(t + 1, acirc_test_input(circ, t),
                                   acirc_ninputs(circ),
                                   acirc_test_output(circ, t), outp,
                                   acirc_noutputs(circ), false))
                ret = ERR;
        } else
            ret = ERR;
        for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
            free(inps[i]);
        if (ret == ERR)
            break;
    }
    if (g_verbose && ret == OK) {
        unsigned long size, resident;
        if (memory(&size, &resident) == OK)
            fprintf(stderr, "memory: %lu MB\n", resident);
    }
    return ret;
}

size_t
mife_run_smart_kappa(const mife_vtable *vt, const acirc_t *circ,
                     const obf_params_t *op, size_t nthreads,
                     aes_randstate_t rng)
{
    long **inps = NULL;
    size_t kappa = 0;
    bool verbosity = g_verbose;

    inps = xcalloc(acirc_nsymbols(circ), sizeof inps[0]);

    if (g_verbose)
        fprintf(stderr, "Choosing κ smartly... ");

    g_verbose = false;
    if (mife_run_setup(&dummy_vtable, vt, circ, op, 8, NULL, NULL, &kappa, nthreads, rng) == ERR)
        goto cleanup;

    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        inps[i] = xcalloc(acirc_symlen(circ, i), sizeof inps[i][0]);
    if (mife_run_all(&dummy_vtable, vt, circ, inps, NULL, &kappa, nthreads, rng) == ERR) {
        fprintf(stderr, "%s: unable to determine κ smartly\n",
                errorstr);
        kappa = 0;
    }
    for (size_t i = 0; i < acirc_nsymbols(circ); ++i)
        free(inps[i]);
cleanup:
    if (inps)
        free(inps);
    g_verbose = verbosity;
    if (g_verbose)
        fprintf(stderr, "%lu\n", kappa);
    return kappa;
}
