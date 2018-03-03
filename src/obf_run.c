#include "obf_run.h"
#include "util.h"

#include <string.h>
#include <mmap/mmap_dummy.h>

int
obf_run_obfuscate(const mmap_vtable *mmap, const obfuscator_vtable *vt,
                  const acirc_t *circ, const obf_params_t *op,
                  const char *fname, size_t secparam, size_t *kappa,
                  size_t nthreads, aes_randstate_t rng)
{
    obfuscation *obf = NULL;
    double start, end, _start, _end;
    int ret = ERR;

    start = current_time();
    _start = current_time();
    obf = vt->obfuscate(mmap, circ, op, secparam, kappa, nthreads, rng);
    if (obf == NULL) {
        fprintf(stderr, "%s: obfuscation failed\n", errorstr);
        goto cleanup;
    }
    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "Obfuscation time: %.2fs\n", _end - _start);
    if (fname) {
        FILE *fp;
        if ((fp = fopen(fname, "w")) == NULL) {
            fprintf(stderr, "%s: unable to open '%s' for writing\n",
                    errorstr, fname);
            goto cleanup;
        }
        _start = current_time();
        if (vt->fwrite(obf, fp) == ERR) {
            fprintf(stderr, "%s: writing obfuscation to disk failed\n",
                    errorstr);
            fclose(fp);
            goto cleanup;
        }
        fclose(fp);
        if (g_verbose)
            fprintf(stderr, "Writing obfuscation to disk: %.2f s [%lu KB]\n",
                    current_time() - _start, filesize(fname) / 1024);
    }
    end = current_time();
    if (g_verbose)
        fprintf(stderr, "Total: %.2fs\n", end - start);
    if (g_verbose) {
        unsigned long size, resident;
        if (memory(&size, &resident) == OK)
            fprintf(stderr, "Memory: %luM\n", resident);
    }
    ret = OK;
cleanup:
    if (obf)
        vt->free(obf);
    return ret;
}

int
obf_run_evaluate(const mmap_vtable *mmap, const obfuscator_vtable *vt,
                 const char *fname, const acirc_t *circ, const long *inputs,
                 size_t ninputs, long *outputs, size_t noutputs, size_t nthreads,
                 size_t *kappa, size_t *npowers)
{
    double start, end, _start, _end;
    obfuscation *obf;
    FILE *fp;
    int ret = ERR;

    if ((fp = fopen(fname, "r")) == NULL) {
        fprintf(stderr, "%s: unable to open '%s' for reading\n",
                errorstr, fname);
        return ERR;
    }

    start = current_time();
    _start = current_time();
    if ((obf = vt->fread(mmap, circ, fp)) == NULL) {
        fprintf(stderr, "%s: reading obfuscator failed\n", errorstr);
        goto cleanup;
    }
    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "Reading obfuscation from disk: %.2fs\n", _end - _start);

    _start = current_time();
    if (vt->evaluate(obf, outputs, noutputs, inputs, ninputs, nthreads, kappa, npowers) == ERR)
        goto cleanup;
    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "Evaluation time: %.2fs\n", _end - _start);

    end = current_time();
    if (g_verbose)
        fprintf(stderr, "Total: %.2fs\n", end - start);
    if (g_verbose) {
        unsigned long size, resident;
        if (memory(&size, &resident) == OK)
            fprintf(stderr, "Memory: %luM\n", resident);
    }
    ret = OK;
cleanup:
    fclose(fp);
    vt->free(obf);
    return ret;
}

size_t
obf_run_smart_kappa(const obfuscator_vtable *vt, const acirc_t *circ,
                    const obf_params_t *op, size_t nthreads,
                    aes_randstate_t rng)
{
    const char *fname = "/tmp/smart-kappa.obf";
    long input[acirc_ninputs(circ)];
    long output[acirc_noutputs(circ)];
    bool verbosity = g_verbose;
    size_t kappa = 0;

    if (g_verbose)
        fprintf(stderr, "Choosing κ smartly...\n");

    g_verbose = false;
    if (obf_run_obfuscate(&dummy_vtable, vt, circ, op, fname, 8, &kappa, nthreads, rng) == ERR) {
        if (g_verbose) fprintf(stderr, "\n");
        fprintf(stderr, "%s: unable to obfuscate to determine smart κ settings\n",
                errorstr);
        kappa = 0;
        goto cleanup;
    }

    memset(input, '\0', sizeof input);
    memset(output, '\0', sizeof output);
    if (obf_run_evaluate(&dummy_vtable, vt, fname, circ, input, acirc_ninputs(circ),
                         output, acirc_noutputs(circ), nthreads, &kappa, NULL) == ERR) {
        if (g_verbose) fprintf(stderr, "\n");
        fprintf(stderr, "%s: unable to evaluate to determine smart κ settings\n",
                errorstr);
        kappa = 0;
        goto cleanup;
    }
    if (g_verbose)
        fprintf(stderr, "%lu\n", kappa);

cleanup:
    g_verbose = verbosity;
    return kappa;
}
