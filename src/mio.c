#include "mmap.h"
#include "obfuscator.h"
#include "util.h"

#include "mife/mife.h"
#include "mife/mife_run.h"
#include "mife/mife_params.h"
#include "lz/obfuscator.h"
#include "mobf/obfuscator.h"
#include "polylog/obfuscator.h"
#include "obf_run.h"

#include <aesrand.h>
#include <acirc.h>
#include <mmap/mmap_clt.h>
#include <mmap/mmap_clt_pl.h>
#include <mmap/mmap_dummy.h>

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *progname = "mio";
static const char *progversion = "v2.0.0 (in progress)";

#define NPOWERS_DEFAULT 1
#define SECPARAM_DEFAULT 8

typedef enum {
    OBF_SCHEME_LZ,
    OBF_SCHEME_CMR,
    OBF_SCHEME_POLYLOG,
} obf_scheme_e;

static int
str_to_obf_scheme(const char *str, obf_scheme_e *scheme)
{
    if (!strcmp(str, "LZ")) {
        *scheme = OBF_SCHEME_LZ;
    } else if (!strcmp(str, "CMR")) {
        *scheme = OBF_SCHEME_CMR;
    } else if (!strcmp(str, "POLYLOG")) {
        *scheme = OBF_SCHEME_POLYLOG;
    } else {
        fprintf(stderr, "%s: unknown obfuscation scheme '%s'\n", errorstr, str);
        return ERR;
    }
    return OK;
}

typedef enum {
    MIFE_SCHEME_CMR,
    MIFE_SCHEME_GC,
} mife_scheme_e;

static int
str_to_mife_scheme(const char *str, mife_scheme_e *scheme)
{
    if (!strcmp(str, "CMR")) {
        *scheme = MIFE_SCHEME_CMR;
    } else if (!strcmp(str, "GC")) {
        *scheme = MIFE_SCHEME_GC;
    } else {
        fprintf(stderr, "%s: unknown scheme '%s'\n", errorstr, str);
        return ERR;
    }
    return OK;
}

static int
str_to_kappa(const char *str, size_t *kappa)
{
    char *endptr;
    *kappa = strtol(str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "%s: invalid κ value given\n", errorstr);
        return ERR;
    }
    if (*kappa == 0) {
        fprintf(stderr, "%s: κ value cannot be zero\n", errorstr);
        return ERR;
    }
    return OK;
}

typedef struct args_t {
    const mmap_vtable *vt;
    char *circuit;
    acirc_t *circ;
    bool smart;
    size_t nthreads;
    bool verbose;
    aes_randstate_t rng;
} args_t;

static void
args_init(args_t *args)
{
    args->vt = &dummy_vtable;
    args->circuit = NULL;
    args->circ = NULL;
    args->smart = false;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->verbose = false;
    aes_randinit(args->rng);
}

static void
args_clear(args_t *args)
{
    if (args->circ)
        acirc_free(args->circ);
    aes_randclear(args->rng);
}

static void
args_usage(void)
{
    char *mmap;
    args_t defaults;

    args_init(&defaults);
    mmap = defaults.vt == &clt_vtable ? "CLT" : "DUMMY";
    printf(
"    --mmap M           set mmap to M (options: CLT, DUMMY | default: %s)\n"
"    --smart            be smart when choosing parameters\n"
"    --nthreads N       set the number of threads to N (default: %lu)\n"
"    --verbose          be verbose\n"
"    --help             print this message and exit\n",
mmap, defaults.nthreads);
    args_clear(&defaults);
}

static int
args_get_size_t(size_t *result, int *argc, char ***argv)
{
    char *endptr;
    if (*argc <= 1)
        return ERR;
    *result = strtol((*argv)[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "%s: invalid argument '%s'\n", errorstr, (*argv)[1]);
        return ERR;
    }
    (*argv)++; (*argc)--;
    return OK;
}

typedef struct {
    size_t secparam;
    size_t npowers;
    mife_scheme_e scheme;
} mife_setup_args_t;

static void
mife_setup_args_init(mife_setup_args_t *args)
{
    args->secparam = SECPARAM_DEFAULT;
    args->npowers = NPOWERS_DEFAULT;
}

static void
mife_setup_usage(bool longform, int ret)
{
    printf("usage: %s mife setup [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf(
"    --secparam λ       set security parameter to λ (default: %d)\n"
"    --npowers N        set the number of powers to N (default: %d)\n"
"    --scheme S         set MIFE scheme to S (options: CMR, GC | default: CMR)\n",
SECPARAM_DEFAULT, NPOWERS_DEFAULT);
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
mife_setup_handle_options(int *argc, char ***argv, void *vargs)
{
    mife_setup_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--secparam")) {
        if (args_get_size_t(&args->secparam, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--npowers")) {
        if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1) return ERR;
        if (str_to_mife_scheme((*argv)[1], &args->scheme) == ERR) return ERR;
        (*argv)++; (*argc)++;
    } else {
        return ERR;
    }
    return OK;
}

static void
mife_encrypt_usage(bool longform, int ret)
{
    printf("usage: %s mife encrypt [<args>] circuit input slot\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static void
mife_decrypt_usage(bool longform, int ret)
{
    printf("usage: %s mife decrypt [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        args_usage();
        printf("\n");
    }
    exit(ret);
}

typedef struct {
    size_t secparam;
    size_t npowers;
    size_t kappa;
    mife_scheme_e scheme;
} mife_test_args_t;

static void
mife_test_args_init(mife_test_args_t *args)
{
    args->npowers = NPOWERS_DEFAULT;
    args->secparam = SECPARAM_DEFAULT;
    args->kappa = 0;
}

static void
mife_test_usage(bool longform, int ret)
{
    printf("usage: %s mife test [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf(
"    --secparam λ       set security parameter to λ (default: %d)\n"
"    --npowers N        set the number of powers to N (default: %d)\n"
"    --scheme S         set MIFE scheme to S (options: CMR, GC | default: CMR)\n"
"    --kappa Κ          set multilinearity to Κ\n",
SECPARAM_DEFAULT, NPOWERS_DEFAULT);
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
mife_test_handle_options(int *argc, char ***argv, void *vargs)
{
    assert(*argc > 0);
    mife_test_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--npowers")) {
        if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--secparam")) {
        if (args_get_size_t(&args->secparam, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--kappa")) {
        if (args_get_size_t(&args->kappa, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1) return ERR;
        if (str_to_mife_scheme((*argv)[1], &args->scheme) == ERR) return ERR;
        (*argv)++; (*argc)--;
    } else {
        return ERR;
    }
    return OK;
}

typedef struct {
    size_t npowers;
} mife_get_kappa_args_t;

static void
mife_get_kappa_args_init(mife_get_kappa_args_t *args)
{
    args->npowers = NPOWERS_DEFAULT;
}

static void
mife_get_kappa_usage(bool longform, int ret)
{
    printf("usage: %s mife get-kappa [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf("    --npowers N        set the number of powers to N (default: %d)\n",
               NPOWERS_DEFAULT);
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
mife_get_kappa_handle_options(int *argc, char ***argv, void *vargs)
{
    assert(*argc > 0);
    mife_get_kappa_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--npowers")) {
        if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
    } else {
        return ERR;
    }
    return OK;
}

typedef struct {
    size_t secparam;
    size_t npowers;
    size_t kappa;
    obf_scheme_e scheme;
} obf_obfuscate_args_t;

static void
obf_obfuscate_args_init(obf_obfuscate_args_t *args)
{
    args->secparam = SECPARAM_DEFAULT;
    args->npowers = NPOWERS_DEFAULT;
    args->scheme = OBF_SCHEME_CMR;
    args->kappa = 0;
}

static void
obf_obfuscate_or_test_usage(bool longform, int ret, const char *cmd)
{
    printf("usage: %s obf %s [<args>] circuit\n", progname, cmd);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf(
"    --secparam λ       set security parameter to λ (default: %d)\n"
"    --npowers N        set the number of powers to N (default: %d)\n"
"    --scheme S         set obfuscation scheme to S (options: CMR, LZ, POLYLOG | default: CMR)\n"
"    --kappa Κ          set multilinearity to Κ\n",
SECPARAM_DEFAULT, NPOWERS_DEFAULT);
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static void
obf_obfuscate_usage(bool longform, int ret)
{
    obf_obfuscate_or_test_usage(longform, ret, "obfuscate");
}

static int
obf_obfuscate_handle_options(int *argc, char ***argv, void *vargs)
{
    obf_obfuscate_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--secparam")) {
        if (args_get_size_t(&args->secparam, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--npowers")) {
        if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1) return ERR;
        if (str_to_obf_scheme((*argv)[1], &args->scheme) == ERR) return ERR;
        (*argv)++; (*argc)--;
    } else if (!strcmp(cmd, "--kappa")) {
        if (str_to_kappa((*argv)[1], &args->kappa) == ERR) return ERR;
        (*argv)++; (*argc)--;
    } else {
        return ERR;
    }
    return OK;
}

typedef struct {
    size_t npowers;
    obf_scheme_e scheme;
} obf_evaluate_args_t;

static void
obf_evaluate_args_init(obf_evaluate_args_t *args)
{
    args->npowers = NPOWERS_DEFAULT;
    args->scheme = OBF_SCHEME_CMR;
}

static void
obf_evaluate_usage(bool longform, int ret)
{
    printf("usage: %s obf evaluate [<args>] circuit input\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf("    --scheme S         set obfuscation scheme to S (options: CMR, LZ, POLYLOG | default: CMR)\n"
               "    --npowers N        set the number of powers to N (default: %d)\n",
               NPOWERS_DEFAULT);
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
obf_evaluate_handle_options(int *argc, char ***argv, void *vargs)
{
    obf_evaluate_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--npowers")) {
        if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1) return ERR;
        if (str_to_obf_scheme((*argv)[1], &args->scheme) == ERR) return ERR;
        (*argv)++; (*argc)--;
    } else {
        return ERR;
    }
    return OK;
}

typedef obf_obfuscate_args_t obf_test_args_t;

#define obf_test_args_init obf_obfuscate_args_init

static void
obf_test_usage(bool longform, int ret)
{
    obf_obfuscate_or_test_usage(longform, ret, "test");
}

#define obf_test_handle_options obf_obfuscate_handle_options

typedef obf_evaluate_args_t obf_get_kappa_args_t;

#define obf_get_kappa_args_init obf_evaluate_args_init

static void
obf_get_kappa_usage(bool longform, int ret)
{
    printf("usage: %s obf get-kappa [<args>] circuit input\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf("    --scheme S         set obfuscation scheme to S (options: CMR, LZ | default: CMR)\n"
               "    --npowers N        set the number of powers to N (default: %d)\n",
               NPOWERS_DEFAULT);
        args_usage();
        printf("\n");
    }
    exit(ret);
}

#define obf_get_kappa_handle_options obf_evaluate_handle_options

static void
handle_options(int *argc, char ***argv, int left, args_t *args, void *others,
               int (*other)(int *, char ***, void *),
               void (*f)(bool, int))
{
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-')
            break;

        if (!strcmp(cmd, "--mmap")) {
            if (*argc <= 1)
                f(false, EXIT_FAILURE);
            const char *mmap = (*argv)[1];
            if (!strcmp(mmap, "CLT")) {
                args->vt = &clt_vtable;
            } else if (!strcmp(mmap, "DUMMY")) {
                args->vt = &dummy_vtable;
            } else {
                fprintf(stderr, "%s: unknown mmap \"%s\"\n", errorstr, mmap);
                f(true, EXIT_FAILURE);
            }
            (*argv)++; (*argc)--;
        } else if (!strcmp(cmd, "--smart")) {
            args->smart = true;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR)
                f(false, EXIT_FAILURE);
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            f(true, EXIT_SUCCESS);
        } else if (other) {
            if (other(argc, argv, others) == ERR)
                f(true, EXIT_FAILURE);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd);
            f(true, EXIT_FAILURE);
        }
        (*argv)++; (*argc)--;
    }
    if (*argc == 0) {
        fprintf(stderr, "%s: missing circuit\n", errorstr);
        f(false, EXIT_FAILURE);
    } else if (*argc < left + 1) {
        fprintf(stderr, "%s: too few arguments\n", errorstr);
        f(false, EXIT_FAILURE);
    } else if (*argc > left + 1) {
        fprintf(stderr, "%s: too many arguments\n", errorstr);
        f(false, EXIT_FAILURE);
    }
    args->circuit = (*argv)[0];
    args->circ = acirc_new(args->circuit, true);
    if (args->circ == NULL) {
        fprintf(stderr, "%s: parsing circuit '%s' failed\n", errorstr, args->circuit);
        exit(EXIT_FAILURE);
    }
    (*argv)++; (*argc)--;
}

static int
mife_select_scheme(acirc_t *circ, op_vtable **op_vt, obf_params_t **op)
{
    *op_vt = &mife_op_vtable;
    *op = (*op_vt)->new(circ, NULL);
    if (*op == NULL) {
        fprintf(stderr, "%s: initializing mife parameters failed\n", errorstr);
        return ERR;
    }
    return OK;
}

static int
cmd_mife_setup(int argc, char **argv, args_t *args)
{
    mife_setup_args_t args_;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    int ret = ERR;

    argv++; argc--;
    mife_setup_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, mife_setup_handle_options, mife_setup_usage);
    if (mife_select_scheme(args->circ, &op_vt, &op) == ERR)
        goto cleanup;
    if (mife_run_setup(args->vt, args->circuit, op, args_.secparam, NULL, args_.npowers,
                       args->nthreads, args->rng) == ERR)
        goto cleanup;
    ret = OK;
cleanup:
    if (op)
        op_vt->free(op);
    return ret;
}

static int
cmd_mife_encrypt(int argc, char **argv, args_t *args)
{
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    size_t slot;
    int ret = ERR;

    argv++; argc--;
    handle_options(&argc, &argv, 2, args, NULL, NULL, mife_encrypt_usage);
    long input[strlen(argv[0])];
    for (size_t i = 0; i < strlen(argv[0]); ++i) {
        if ((input[i] = char_to_long(argv[0][i])) < 0)
            goto cleanup;
    }
    if (args_get_size_t(&slot, &argc, &argv) == ERR)
        goto cleanup;
    if (mife_select_scheme(args->circ, &op_vt, &op) == ERR)
        goto cleanup;
    if (mife_run_encrypt(args->vt, args->circuit, op, input, slot,
                         args->nthreads, NULL, args->rng) == ERR)
        goto cleanup;
    ret = OK;
cleanup:
    if (op)
        op_vt->free(op);
    return ret;
}

static int
cmd_mife_decrypt(int argc, char **argv, args_t *args)
{
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    char *ek = NULL;
    char **cts = NULL;
    long *rop = NULL;
    size_t length, nslots = 0;
    int ret = ERR;

    argv++; argc--;
    handle_options(&argc, &argv, 0, args, NULL, NULL, mife_decrypt_usage);
    if (mife_select_scheme(args->circ, &op_vt, &op) == ERR)
        goto cleanup;
    nslots = op->cp.nslots;

    length = snprintf(NULL, 0, "%s.ek\n", args->circuit);
    ek = my_calloc(length, sizeof ek[0]);
    snprintf(ek, length, "%s.ek\n", args->circuit);
    cts = my_calloc(nslots, sizeof cts[0]);
    for (size_t i = 0; i < nslots; ++i) {
        length = snprintf(NULL, 0, "%s.%lu.ct\n", args->circuit, i);
        cts[i] = my_calloc(length, sizeof cts[i][0]);
        (void) snprintf(cts[i], length, "%s.%lu.ct\n", args->circuit, i);
    }
    rop = my_calloc(acirc_noutputs(op->cp.circ), sizeof rop[0]);
    if (mife_run_decrypt(ek, cts, rop, args->vt, op, NULL, args->nthreads) == ERR) {
        fprintf(stderr, "%s: mife decrypt failed\n", errorstr);
        goto cleanup;
    }
    printf("result: ");
    for (size_t o = 0; o < acirc_noutputs(op->cp.circ); ++o) {
        printf("%ld", rop[o]);
    }
    printf("\n");
    ret = OK;
cleanup:
    if (ek)
        free(ek);
    if (cts) {
        for (size_t i = 0; i < nslots; ++i)
            free(cts[i]);
        free(cts);
    }
    if (op)
        op_vt->free(op);
    if (rop)
        free(rop);
    return ret;
}

static int
cmd_mife_test(int argc, char **argv, args_t *args)
{
    mife_test_args_t args_;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    size_t kappa = 0;
    int ret = ERR;

    argv++; argc--;
    mife_test_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, mife_test_handle_options, mife_test_usage);
    if (mife_select_scheme(args->circ, &op_vt, &op) == ERR)
        goto cleanup;
    if (args_.kappa)
        kappa = args_.kappa;
    if (args->smart) {
        kappa = mife_run_smart_kappa(args->circuit, op, args_.npowers, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    }
    if (mife_run_test(args->vt, args->circuit, op, args_.secparam,
                      &kappa, args_.npowers, args->nthreads, args->rng) == ERR) {
        fprintf(stderr, "%s: mife test failed\n", errorstr);
        goto cleanup;
    }
    ret = OK;
cleanup:
    if (op)
        op_vt->free(op);
    return ret;
}

static int
cmd_mife_get_kappa(int argc, char **argv, args_t *args)
{
    mife_get_kappa_args_t args_;
    const mmap_vtable *vt = &dummy_vtable;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    size_t kappa = 0;
    int ret = ERR;

    argv++, argc--;
    mife_get_kappa_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, mife_get_kappa_handle_options, mife_get_kappa_usage);
    if (mife_select_scheme(args->circ, &op_vt, &op) == ERR)
        goto cleanup;
    if (args->smart) {
        kappa = mife_run_smart_kappa(args->circuit, op, args_.npowers, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    } else {
        if (mife_run_setup(vt, args->circuit, op, 8, &kappa, args_.npowers,
                           args->nthreads, args->rng) == ERR) {
            fprintf(stderr, "%s: mife setup failed\n", errorstr);
            goto cleanup;
        }
    }
    printf("κ = %lu\n", kappa);
    ret = OK;
cleanup:
    if (op)
        op_vt->free(op);
    return ret;
}

static void
mife_usage(bool longform, int ret)
{
    printf("usage: %s mife <command> [<args>]\n", progname);
    if (longform) {
        printf("\nAvailable commands:\n\n"
               "   setup         run setup routine\n"
               "   encrypt       run encryption routine\n"
               "   decrypt       run decryption routine\n"
               "   test          run test suite\n"
               "   get-kappa     get κ value\n"
               "   help          print this message and exit\n\n");
    }
    exit(ret);
}

static int
cmd_mife(int argc, char **argv)
{
    args_t args;
    int ret = ERR;

    if (argc == 1)
        mife_usage(true, EXIT_FAILURE);

    const char *const cmd = argv[1];
    args_init(&args);

    argv++; argc--;
    if (!strcmp(cmd, "setup")) {
        ret = cmd_mife_setup(argc, argv, &args);
    } else if (!strcmp(cmd, "encrypt")) {
        ret = cmd_mife_encrypt(argc, argv, &args);
    } else if (!strcmp(cmd, "decrypt")) {
        ret = cmd_mife_decrypt(argc, argv, &args);
    } else if (!strcmp(cmd, "test")) {
        ret = cmd_mife_test(argc, argv, &args);
    } else if (!strcmp(cmd, "get-kappa")) {
        ret = cmd_mife_get_kappa(argc, argv, &args);
    } else if (!strcmp(cmd, "help")
               || !strcmp(cmd, "--help")
               || !strcmp(cmd, "-h")) {
        mife_usage(true, EXIT_SUCCESS);
    } else {
        fprintf(stderr, "%s: unknown command '%s'\n", errorstr, cmd);
        mife_usage(true, EXIT_FAILURE);
    }
    args_clear(&args);
    return ret;
}

static int
obf_select_scheme(obf_scheme_e scheme, acirc_t *circ, size_t npowers,
                  obfuscator_vtable **vt, op_vtable **op_vt, obf_params_t **op)
{
    lz_obf_params_t lz_params;
    mobf_obf_params_t mobf_params;
    void *vparams;

    switch (scheme) {
    case OBF_SCHEME_CMR:
        *vt = &mobf_obfuscator_vtable;
        *op_vt = &mobf_op_vtable;
        mobf_params.npowers = npowers;
        vparams = &mobf_params;
        break;
    case OBF_SCHEME_LZ:
        *vt = &lz_obfuscator_vtable;
        *op_vt = &lz_op_vtable;
        lz_params.npowers = npowers;
        vparams = &lz_params;
        break;
    case OBF_SCHEME_POLYLOG:
        *vt = &polylog_obfuscator_vtable;
        *op_vt = &polylog_op_vtable;
        vparams = NULL;
        break;
    }

    *op = (*op_vt)->new(circ, vparams);
    if (*op == NULL) {
        fprintf(stderr, "%s: initializing obfuscation parameters failed\n", errorstr);
        return ERR;
    }
    return OK;
}

static int
cmd_obf_obfuscate(int argc, char **argv, args_t *args)
{
    obf_obfuscate_args_t args_;
    obfuscator_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    char *fname = NULL;
    size_t length, kappa = 0;
    int ret = ERR;

    argv++; argc--;
    obf_obfuscate_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, obf_obfuscate_handle_options,
                   obf_obfuscate_usage);
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;

    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    if ((fname = calloc(length, sizeof fname[0])) == NULL)
        goto cleanup;
    snprintf(fname, length, "%s.obf", args->circuit);

    if (args_.scheme == OBF_SCHEME_POLYLOG && args->vt == &clt_vtable)
        args->vt = &clt_pl_vtable;
    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    }
    if (args_.kappa)
        kappa = args_.kappa;

    if (obf_run_obfuscate(args->vt, vt, fname, op, args_.secparam, &kappa,
                          args->nthreads, args->rng) == ERR)
        goto cleanup;

    ret = OK;
cleanup:
    if (fname)
        free(fname);
    if (op)
        op_vt->free(op);
    return ret;
}

static int
cmd_obf_evaluate(int argc, char **argv, args_t *args)
{
    obf_evaluate_args_t args_;
    obfuscator_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    long *input = NULL, *output = NULL;
    char *fname = NULL;
    size_t length;
    int ret = ERR;

    argv++; argc--;
    obf_evaluate_args_init(&args_);
    handle_options(&argc, &argv, 1, args, &args_, obf_evaluate_handle_options,
                   obf_evaluate_usage);
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;

    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    if ((fname = my_calloc(length, sizeof fname[0])) == NULL)
        goto cleanup;
    snprintf(fname, length, "%s.obf", args->circuit);

    if (args_.scheme == OBF_SCHEME_POLYLOG && args->vt == &clt_vtable)
        args->vt = &clt_pl_vtable;
    if ((input = my_calloc(strlen(argv[0]), sizeof input[0])) == NULL)
        goto cleanup;
    if ((output = my_calloc(acirc_noutputs(op->cp.circ), sizeof output[0])) == NULL)
        goto cleanup;

    for (size_t i = 0; i < strlen(argv[0]); ++i) {
        if ((input[i] = char_to_long(argv[0][i])) < 0)
            goto cleanup;
    }
    if (obf_run_evaluate(args->vt, vt, fname, op, input, strlen(argv[0]), output,
                         acirc_noutputs(op->cp.circ), args->nthreads, NULL, NULL) == ERR)
        goto cleanup;

    printf("result: ");
    for (size_t i = 0; i < acirc_noutputs(op->cp.circ); ++i)
        printf("%c", long_to_char(output[i]));
    printf("\n");

    ret = OK;
cleanup:
    if (input)
        free(input);
    if (output)
        free(output);
    if (fname)
        free(fname);
    if (op)
        op_vt->free(op);
    return ret;
}

static int
cmd_obf_test(int argc, char **argv, args_t *args)
{
    obf_test_args_t args_;
    obfuscator_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    char *fname = NULL;
    size_t length, kappa = 0;
    bool passed = true;
    int ret = ERR;

    argv++; argc--;
    obf_test_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, obf_test_handle_options, obf_test_usage);
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;
    if (args_.scheme == OBF_SCHEME_POLYLOG && args->vt == &clt_vtable)
        args->vt = &clt_pl_vtable;
    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    }
    if (args_.kappa)
        kappa = args_.kappa;

    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    if ((fname = my_calloc(length, sizeof fname[0])) == NULL)
        goto cleanup;
    snprintf(fname, length, "%s.obf", args->circuit);
    if (obf_run_obfuscate(args->vt, vt, fname, op, args_.secparam, &kappa,
                          args->nthreads, args->rng) == ERR)
        goto cleanup;

    for (size_t t = 0; t < acirc_ntests(args->circ); ++t) {
        long outp[acirc_noutputs(op->cp.circ)];
        if (obf_run_evaluate(args->vt, vt, fname, op, acirc_test_input(args->circ, t),
                             acirc_ninputs(args->circ), outp, acirc_noutputs(args->circ),
                             args->nthreads, &kappa, NULL) == ERR)
            goto cleanup;
        if (!print_test_output(t + 1, acirc_test_input(args->circ, t), acirc_ninputs(args->circ),
                               acirc_test_output(args->circ, t), outp, acirc_noutputs(args->circ),
                               false))
            passed = false;
    }
    if (passed)
        ret = OK;
cleanup:
    if (fname)
        free(fname);
    if (op)
        op_vt->free(op);
    return ret;
}

static int
cmd_obf_get_kappa(int argc, char **argv, args_t *args)
{
    obf_get_kappa_args_t args_;
    obfuscator_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    size_t kappa = 0;
    int ret = ERR;

    argv++, argc--;
    obf_get_kappa_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, obf_get_kappa_handle_options, obf_get_kappa_usage);
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;

    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    } else {
        if (obf_run_obfuscate(args->vt, vt, NULL, op, 8, &kappa,
                              args->nthreads, args->rng) == ERR)
            goto cleanup;
    }
    printf("κ = %lu\n", kappa);
    ret = OK;
cleanup:
    if (op)
        op_vt->free(op);
    return ret;
}

static void
obf_usage(bool longform, int ret)
{
    printf("usage: %s obf <command> [<args>]\n", progname);
    if (longform) {
        printf("\nAvailable commands:\n\n"
               "   obfuscate    run circuit obfuscation\n"
               "   evaluate     run circuit evaluation\n"
               "   test         run test suite\n"
               "   get-kappa    get κ value\n"
               "   help         print this message and exit\n\n");
    }
    exit(ret);
}

static int
cmd_obf(int argc, char **argv)
{
    args_t args;
    int ret = ERR;

    if (argc == 1)
        obf_usage(true, EXIT_FAILURE);

    const char *const cmd = argv[1];
    args_init(&args);

    argv++; argc--;
    if (!strcmp(cmd, "obfuscate")) {
        ret = cmd_obf_obfuscate(argc, argv, &args);
    } else if (!strcmp(cmd, "evaluate")) {
        ret = cmd_obf_evaluate(argc, argv, &args);
    } else if (!strcmp(cmd, "test")) {
        ret = cmd_obf_test(argc, argv, &args);
    } else if (!strcmp(cmd, "get-kappa")) {
        ret = cmd_obf_get_kappa(argc, argv, &args);
    } else if (!strcmp(cmd, "help")
               || !strcmp(cmd, "--help")
               || !strcmp(cmd, "-h")) {
        obf_usage(true, EXIT_SUCCESS);
    } else {
        fprintf(stderr, "%s: unknown command '%s'\n", errorstr, cmd);
        obf_usage(true, EXIT_FAILURE);
    }
    args_clear(&args);
    return ret;
}

static void
usage(bool longform, int ret)
{
    printf("%s %s — Circuit-based MIFE + Obfuscation\n\n", progname, progversion);
    printf("usage: %s <command> [<args>]\n", progname);
    if (longform) {
        printf("\nSupported MIFE schemes:\n");
        printf("· CMR:     CMR MIFE scheme [http://ia.cr/2017/826, §5.1]\n");
        printf("· GC:      Garbled-circuit-based scheme\n");
        printf("\n");
        printf("Supported obfuscation schemes:\n");
        printf("· CMR:     CMR obfuscation scheme [http://ia.cr/2017/826, §5.4]\n");
        printf("· LZ:      Linnerman scheme       [http://ia.cr/2017/826, §B]\n");
        printf("· POLYLOG: Obfuscation using polylog CLT\n");
        printf("\nAvailable commands:\n"
               "   mife       run multi-input functional encryption\n"
               "   obf        run program obfuscation\n"
               "   help       print this message and exit\n\n");
    }
    exit(ret);
}

int
main(int argc, char **argv)
{
    const char *const command = argv[1];
    int ret = ERR;

    if (argc == 1)
        usage(true, EXIT_SUCCESS);

    argv++; argc--;

    if (!strcmp(command, "mife")) {
        ret = cmd_mife(argc, argv);
    } else if (!strcmp(command, "obf")) {
        ret = cmd_obf(argc, argv);
    } else if (!strcmp(command, "help")
               || !strcmp(command, "--help")
               || !strcmp(command, "-h")) {
        usage(true, EXIT_SUCCESS);
    } else {
        fprintf(stderr, "%s: unknown command '%s'\n", errorstr, command);
        usage(true, EXIT_FAILURE);
    }
    return ret == OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
