#include "mmap.h"
#include "obfuscator.h"
#include "util.h"

#include "mife/mife.h"
#include "mife/mife_run.h"
#include "mife/mife_params.h"
/* #include "lin/obfuscator.h" */
#include "lz/obfuscator.h"
#include "mobf/obfuscator.h"
#include "obf_run.h"

#include <aesrand/aesrand.h>
#include <acirc.h>
#include <mmap/mmap_clt.h>
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

static char *progname = "mio";
static char *progversion = "v2.0";

#define NPOWERS_DEFAULT 1
#define SECPARAM_DEFAULT 8
#define BASE_DEFAULT 2

enum scheme_e {
    SCHEME_LIN,
    SCHEME_LZ,
    SCHEME_MIFE,
};

typedef struct args_t {
    char *circuit;
    acirc_t *circ;
    size_t secparam;
    const mmap_vtable *vt;
    bool smart;
    bool sigma;
    size_t symlen;
    size_t base;
    size_t nthreads;
    bool verbose;
    aes_randstate_t rng;
} args_t;

static void
args_init(args_t *args)
{
    args->circuit = NULL;
    args->circ = NULL;
    args->secparam = 16;
    args->vt = &dummy_vtable;
    args->smart = false;
    args->sigma = false;
    args->symlen = 1;
    args->base = 2;
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
"    --mmap STR         set mmap to STR (options: CLT, DUMMY | default: %s)\n"
"    --smart            be smart when choosing parameters\n"
"    --sigma            use Σ-vectors (default: %s)\n"
"    --symlen N         set Σ-vector length to N bits (default: %lu)\n"
"    --base B           set base to B (default: %lu)\n"
"    --nthreads N       set the number of threads to N (default: %lu)\n"
"    --verbose          be verbose\n"
"    --help             print this message and exit\n",
mmap, defaults.sigma ? "yes" : "no", defaults.symlen, defaults.base, defaults.nthreads);
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
        fprintf(stderr, "error: invalid argument '%s'\n", (*argv)[1]);
        return ERR;
    }
    (*argv)++; (*argc)--;
    return OK;
}

typedef struct mife_setup_args_t {
    size_t secparam;
    size_t npowers;
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
        printf("    --secparam λ       set security parameter to λ (default: %d)\n"
               "    --npowers N        set the number of powers to N (default: %d)\n",
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
        if (args_get_size_t(&args->secparam, argc, argv) == ERR)
            return ERR;
    } else if (!strcmp(cmd, "--npowers")) {
        if (args_get_size_t(&args->npowers, argc, argv) == ERR)
            return ERR;
    } else {
        return ERR;
    }
    return OK;
}

typedef struct {
} mife_encrypt_args_t;

static void
mife_encrypt_args_init(mife_encrypt_args_t *args)
{
    (void) args;
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

static int
mife_encrypt_handle_options(int *argc, char ***argv, void *vargs)
{
    (void) argc; (void) argv; (void) vargs;
    return ERR;
}

typedef struct {
} mife_decrypt_args_t;

static void
mife_decrypt_args_init(mife_decrypt_args_t *args)
{
    (void) args;
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

static int
mife_decrypt_handle_options(int *argc, char ***argv, void *vargs)
{
    (void) argc; (void) argv; (void) vargs;
    return ERR;
}

typedef struct {
    size_t secparam;
    size_t npowers;
} mife_test_args_t;

static void
mife_test_args_init(mife_test_args_t *args)
{
    args->npowers = NPOWERS_DEFAULT;
    args->secparam = SECPARAM_DEFAULT;
}

static void
mife_test_usage(bool longform, int ret)
{
    printf("usage: %s mife test [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf("    --secparam λ       set security parameter to λ (default: %d)\n"
               "    --npowers N        set the number of powers to N (default: %d)\n",
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
        if (args_get_size_t(&args->npowers, argc, argv) == ERR)
            return ERR;
    } else if (!strcmp(cmd, "--secparam")) {
        if (args_get_size_t(&args->secparam, argc, argv) == ERR)
            return ERR;
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
        if (args_get_size_t(&args->npowers, argc, argv) == ERR)
            return ERR;
    } else {
        return ERR;
    }
    return OK;
}

typedef struct {
    size_t secparam;
    size_t npowers;
    size_t kappa;
    enum scheme_e scheme;
} obf_obfuscate_args_t;

static void
obf_obfuscate_args_init(obf_obfuscate_args_t *args)
{
    args->secparam = SECPARAM_DEFAULT;
    args->npowers = NPOWERS_DEFAULT;
    args->scheme = SCHEME_MIFE;
    args->kappa = 0;
}

static void
obf_obfuscate_or_test_usage(bool longform, int ret, const char *cmd)
{
    printf("usage: %s obf %s [<args>] circuit\n", progname, cmd);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf(
            "    --kappa Κ          set kappa to Κ\n"
            "    --scheme S         set obfuscation scheme to S (options: LIN, LZ, MIFE | default: MIFE)\n"
            "    --secparam λ       set security parameter to λ (default: %d)\n"
            "    --npowers N        set the number of powers to N (default: %d)\n",
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
        if (args_get_size_t(&args->secparam, argc, argv) == ERR)
            return ERR;
    } else if (!strcmp(cmd, "--npowers")) {
        if (args_get_size_t(&args->npowers, argc, argv) == ERR)
            return ERR;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1)
            return ERR;
        const char *scheme = (*argv)[1];
        if (!strcmp(scheme, "LIN")) {
            args->scheme = SCHEME_LIN;
        } else if (!strcmp(scheme, "LZ")) {
            args->scheme = SCHEME_LZ;
        } else if (!strcmp(scheme, "MIFE")) {
            args->scheme = SCHEME_MIFE;
        } else {
            fprintf(stderr, "error: unknown scheme '%s'\n", scheme);
            return ERR;
        }
        (*argv)++; (*argc)--;
    } else if (!strcmp(cmd, "--kappa")) {
        if (*argc <= 1)
            return ERR;
        const char *str = (*argv)[1];
        char *endptr;
        args->kappa = strtol(str, &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "error: invalid κ value given\n");
            return ERR;
        }
        if (args->kappa == 0) {
            fprintf(stderr, "error: κ value cannot be zero\n");
            return ERR;
        }
        (*argv)++; (*argc)--;
    } else {
        return ERR;
    }
    return OK;
}

typedef struct {
    size_t npowers;
    enum scheme_e scheme;
} obf_evaluate_args_t;

static void
obf_evaluate_args_init(obf_evaluate_args_t *args)
{
    args->npowers = NPOWERS_DEFAULT;
    args->scheme = SCHEME_MIFE;
}

static void
obf_evaluate_usage(bool longform, int ret)
{
    printf("usage: %s obf evaluate [<args>] circuit input\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        printf("    --scheme S         set obfuscation scheme to S (options: LZ, MIFE | default: MIFE)\n"
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
        if (args_get_size_t(&args->npowers, argc, argv) == ERR)
            return ERR;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1)
            return ERR;
        const char *scheme = (*argv)[1];
        if (!strcmp(scheme, "LIN")) {
            args->scheme = SCHEME_LIN;
        } else if (!strcmp(scheme, "LZ")) {
            args->scheme = SCHEME_LZ;
        } else if (!strcmp(scheme, "MIFE")) {
            args->scheme = SCHEME_MIFE;
        } else {
            fprintf(stderr, "error: unknown scheme '%s'\n", scheme);
            return ERR;
        }
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
        printf("    --scheme S         set obfuscation scheme to S (options: LZ, MIFE | default: MIFE)\n"
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

        if (!strcmp(cmd, "--secparam")) {
            if (args_get_size_t(&args->secparam, argc, argv) == ERR)
                f(false, EXIT_FAILURE);
        } else if (!strcmp(cmd, "--mmap")) {
            if (*argc <= 1)
                f(false, EXIT_FAILURE);
            const char *mmap = (*argv)[1];
            if (!strcmp(mmap, "CLT")) {
                args->vt = &clt_vtable;
            } else if (!strcmp(mmap, "DUMMY")) {
                args->vt = &dummy_vtable;
            } else {
                fprintf(stderr, "error: unknown mmap \"%s\"\n", mmap);
                f(true, EXIT_FAILURE);
            }
            (*argv)++; (*argc)--;
        } else if (!strcmp(cmd, "--smart")) {
            args->smart = true;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR)
                f(false, EXIT_FAILURE);
        } else if (!strcmp(cmd, "--sigma")) {
            args->sigma = true;
        } else if (!strcmp(cmd, "--symlen")) {
            if (args_get_size_t(&args->symlen, argc, argv) == ERR)
                f(false, EXIT_FAILURE);
        } else if (!strcmp(cmd, "--base")) {
            if (args_get_size_t(&args->base, argc, argv) == ERR)
                f(false, EXIT_FAILURE);
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            f(true, EXIT_SUCCESS);
        } else if (other) {
            if (other(argc, argv, others) == ERR)
                f(true, EXIT_FAILURE);
        } else {
            fprintf(stderr, "error: unknown argument '%s'\n", cmd);
            f(true, EXIT_FAILURE);
        }
        (*argv)++; (*argc)--;
    }
    if (*argc == 0) {
        fprintf(stderr, "error: missing circuit\n");
        f(false, EXIT_FAILURE);
    } else if (*argc < left + 1) {
        fprintf(stderr, "error: too few arguments\n");
        f(false, EXIT_FAILURE);
    } else if (*argc > left + 1) {
        fprintf(stderr, "error: too many arguments\n");
        f(false, EXIT_FAILURE);
    }
    args->circuit = (*argv)[0];
    args->circ = acirc_new(args->circuit);
    if (args->circ == NULL) {
        fprintf(stderr, "error: parsing circuit '%s' failed\n", args->circuit);
        exit(EXIT_FAILURE);
    }
    (*argv)++; (*argc)--;
}

/*******************************************************************************/

static int
mife_select_scheme(acirc_t *circ, bool sigma, size_t symlen, size_t base,
                   op_vtable **op_vt, obf_params_t **op)
{
    mife_params_t params;
    void *vparams;

    params.symlen = symlen;
    params.sigma = sigma;
    params.base = base;
    vparams = &params;

    *op_vt = &mife_op_vtable;
    *op = (*op_vt)->new(circ, vparams);
    if (*op == NULL) {
        fprintf(stderr, "error: initializing mife parameters failed\n");
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
    if (mife_select_scheme(args->circ, args->sigma, args->symlen, args->base, &op_vt, &op) == ERR)
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
    mife_encrypt_args_t args_;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    size_t slot;
    int ret = ERR;

    argv++; argc--;
    mife_encrypt_args_init(&args_);
    handle_options(&argc, &argv, 2, args, &args_, mife_encrypt_handle_options, mife_encrypt_usage);
    long input[strlen(argv[0])];
    for (size_t i = 0; i < strlen(argv[0]); ++i) {
        if ((input[i] = char_to_long(argv[0][i])) < 0)
            goto cleanup;
    }
    if (args_get_size_t(&slot, &argc, &argv) == ERR)
        goto cleanup;
    if (mife_select_scheme(args->circ, args->sigma, args->symlen, args->base, &op_vt, &op) == ERR)
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
    mife_decrypt_args_t args_;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    char *ek = NULL;
    char **cts = NULL;
    long *rop = NULL;
    size_t length, nslots = 0;
    int ret = ERR;

    argv++; argc--;
    mife_decrypt_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, mife_decrypt_handle_options, mife_decrypt_usage);
    if (mife_select_scheme(args->circ, args->sigma, args->symlen, args->base, &op_vt, &op) == ERR)
        goto cleanup;
    nslots = op->cp.n;

    length = snprintf(NULL, 0, "%s.ek\n", args->circuit);
    ek = my_calloc(length, sizeof ek[0]);
    snprintf(ek, length, "%s.ek\n", args->circuit);
    cts = my_calloc(nslots, sizeof cts[0]);
    for (size_t i = 0; i < nslots; ++i) {
        length = snprintf(NULL, 0, "%s.%lu.ct\n", args->circuit, i);
        cts[i] = my_calloc(length, sizeof cts[i][0]);
        (void) snprintf(cts[i], length, "%s.%lu.ct\n", args->circuit, i);
    }
    rop = my_calloc(op->cp.m, sizeof rop[0]);
    if (mife_run_decrypt(ek, cts, rop, args->vt, op, NULL, args->nthreads) == ERR) {
        fprintf(stderr, "error: mife decrypt failed\n");
        goto cleanup;
    }
    printf("result: ");
    for (size_t o = 0; o < op->cp.m; ++o) {
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
    if (mife_select_scheme(args->circ, args->sigma, args->symlen, args->base, &op_vt, &op) == ERR)
        goto cleanup;
    if (args->smart) {
        kappa = mife_run_smart_kappa(args->circuit, op, args_.npowers, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    }
    if (mife_run_test(args->vt, args->circuit, op, args_.secparam,
                      &kappa, args_.npowers, args->nthreads, args->rng) == ERR) {
        fprintf(stderr, "error: mife test failed\n");
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
    if (mife_select_scheme(args->circ, args->sigma, args->symlen, args->base, &op_vt, &op) == ERR)
        goto cleanup;
    if (args->smart) {
        kappa = mife_run_smart_kappa(args->circuit, op, args_.npowers, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    } else {
        if (mife_run_setup(vt, args->circuit, op, args->secparam, &kappa, args_.npowers,
                           args->nthreads, args->rng) == ERR) {
            fprintf(stderr, "error: mife setup failed\n");
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
               "   setup         run MIFE setup routine\n"
               "   encrypt       run MIFE encryption routine\n"
               "   decrypt       run MIFE decryption routine\n"
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
        fprintf(stderr, "error: unknown command '%s'\n", cmd);
        mife_usage(true, EXIT_FAILURE);
    }
    args_clear(&args);
    return ret;
}

/*******************************************************************************/

static int
obf_select_scheme(enum scheme_e scheme, acirc_t *circ, size_t npowers, bool sigma,
                  size_t symlen, size_t base, obfuscator_vtable **vt,
                  op_vtable **op_vt, obf_params_t **op)
{
    /* lin_obf_params_t lin_params; */
    lz_obf_params_t lz_params;
    mobf_obf_params_t mobf_params;
    void *vparams;

    if (base != 2 && scheme != SCHEME_MIFE) {
        fprintf(stderr, "error: base != 2 only supported with MIFE obfuscation scheme\n");
        return ERR;
    }

    switch (scheme) {
    case SCHEME_LIN:
        fprintf(stderr, "error: LIN no longer supported\n");
        return ERR;
    /*     *vt = &lin_obfuscator_vtable; */
    /*     *op_vt = &lin_op_vtable; */
    /*     lin_params.symlen = symlen; */
    /*     lin_params.sigma = sigma; */
    /*     vparams = &lin_params; */
    /*     break; */
    case SCHEME_LZ:
        *vt = &lz_obfuscator_vtable;
        *op_vt = &lz_op_vtable;
        lz_params.npowers = npowers;
        lz_params.symlen = symlen;
        lz_params.sigma = sigma;
        vparams = &lz_params;
        break;
    case SCHEME_MIFE:
        *vt = &mobf_obfuscator_vtable;
        *op_vt = &mobf_op_vtable;
        mobf_params.npowers = npowers;
        mobf_params.symlen = symlen;
        mobf_params.sigma = sigma;
        mobf_params.base = base;
        vparams = &mobf_params;
        break;
    default:
        return ERR;
    }

    *op = (*op_vt)->new(circ, vparams);
    if (*op == NULL) {
        fprintf(stderr, "error: initializing obfuscation parameters failed\n");
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
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers, args->sigma,
                          args->symlen, args->base, &vt, &op_vt, &op) == ERR)
        goto cleanup;
    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    }
    if (args_.kappa)
        kappa = args_.kappa;

    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    fname = my_calloc(length, sizeof fname[0]);
    snprintf(fname, length, "%s.obf", args->circuit);
    if (obf_run_obfuscate(args->vt, vt, fname, op, args->secparam, &kappa,
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
    handle_options(&argc, &argv, 1, args, &args_, obf_evaluate_handle_options, obf_evaluate_usage);
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers, args->sigma,
                          args->symlen, args->base, &vt, &op_vt, &op) == ERR)
        goto cleanup;

    input = my_calloc(strlen(argv[0]), sizeof input[0]);
    output = my_calloc(op->cp.m, sizeof output[0]);
    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    fname = my_calloc(length, sizeof fname[0]);
    snprintf(fname, length, "%s.obf", args->circuit);

    for (size_t i = 0; i < strlen(argv[0]); ++i) {
        if ((input[i] = char_to_long(argv[0][i])) < 0)
            goto cleanup;
    }
    if (obf_run_evaluate(args->vt, vt, fname, op, input, strlen(argv[0]), output,
                         op->cp.m, args->nthreads, NULL, NULL) == ERR)
        goto cleanup;

    printf("result: ");
    for (size_t i = 0; i < op->cp.m; ++i)
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
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers, args->sigma,
                          args->symlen, args->base, &vt, &op_vt, &op) == ERR)
        goto cleanup;
    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    }
    if (args_.kappa)
        kappa = args_.kappa;


    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    fname = my_calloc(length, sizeof fname[0]);
    snprintf(fname, length, "%s.obf", args->circuit);
    if (obf_run_obfuscate(args->vt, vt, fname, op, args->secparam, &kappa,
                          args->nthreads, args->rng) == ERR)
        goto cleanup;

    for (size_t t = 0; t < acirc_ntests(args->circ); ++t) {
        long outp[op->cp.m];
        if (obf_run_evaluate(args->vt, vt, fname, op, acirc_test_input(args->circ, t),
                             acirc_ninputs(args->circ), outp, acirc_noutputs(args->circ),
                             args->nthreads, &kappa, NULL) == ERR)
            goto cleanup;
        if (!print_test_output(t + 1, acirc_test_input(args->circ, t), acirc_ninputs(args->circ),
                               acirc_test_output(args->circ, t), outp, acirc_noutputs(args->circ),
                               args_.scheme == SCHEME_LIN))
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
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers, args->sigma,
                          args->symlen, args->base, &vt, &op_vt, &op) == ERR)
        goto cleanup;

    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            goto cleanup;
    } else {
        if (obf_run_obfuscate(args->vt, vt, NULL, op, args->secparam, &kappa,
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
        fprintf(stderr, "error: unknown command '%s'\n", cmd);
        obf_usage(true, EXIT_FAILURE);
    }
    args_clear(&args);
    return ret;
}

static void
usage(bool longform, int ret)
{
    printf("usage: %s <command> [<args>]\n", progname);
    if (longform) {
        printf("\nAvailable commands:\n"
               "   mife       run multi-input functional encryption\n"
               "   obf        run program obfuscation\n"
               "   version    print version information and exit\n"
               "   help       print this message and exit\n\n");
    }
    exit(ret);
}

int
main(int argc, char **argv)
{
    const char *const command = argv[1];
    int ret = ERR;

    if (argc == 1) {
        usage(false, EXIT_FAILURE);
    }

    argv++; argc--;

    if (!strcmp(command, "mife")) {
        ret = cmd_mife(argc, argv);
    } else if (!strcmp(command, "obf")) {
        ret = cmd_obf(argc, argv);
    } else if (!strcmp(command, "help")
               || !strcmp(command, "--help")
               || !strcmp(command, "-h")) {
        usage(true, EXIT_SUCCESS);
    } else if (!strcmp(command, "version")) {
        printf("%s %s\n", progname, progversion);
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "error: unknown command '%s'\n", command);
        usage(true, EXIT_FAILURE);
    }
    return ret == OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
