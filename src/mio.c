#include "mmap.h"
#include "obfuscator.h"
#include "util.h"

#include "mife/mife_run.h"
#include "mife/mife_params.h"
/* #include "lin/obfuscator.h" */
#include "lz/obfuscator.h"
#include "mobf/obfuscator.h"
#include "obf_run.h"

#include <aesrand.h>
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
static char *progversion = "v1.0.0 (in progress)";

enum scheme_e {
    SCHEME_LIN,
    SCHEME_LZ,
    SCHEME_MIFE,
};

static char *
scheme_to_string(enum scheme_e scheme)
{
    switch (scheme) {
    case SCHEME_LIN:
        return "LIN";
    case SCHEME_LZ:
        return "LZ";
    case SCHEME_MIFE:
        return "MIFE";
    }
    abort();
}

typedef struct args_t {
    char *circuit;
    acirc circ;
    circ_params_t cp;
    size_t secparam;
    const mmap_vtable *vt;
    bool smart;
    bool sigma;
    size_t symlen;
    size_t nthreads;
    bool verbose;
    aes_randstate_t rng;
} args_t;

static void
args_init(args_t *args)
{
    args->circuit = NULL;
    args->secparam = 16;
    args->vt = &dummy_vtable;
    args->smart = false;
    args->sigma = false;
    args->symlen = 1;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->verbose = false;
    aes_randinit(args->rng);
}

static void
args_clear(args_t *args)
{
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
"    --nthreads N       set the number of threads to N (default: %lu)\n"
"    --verbose          be verbose\n"
"    --help             print this message and exit\n",
mmap, defaults.sigma ? "yes" : "no", defaults.symlen, defaults.nthreads
        );
    args_clear(&defaults);
}

typedef struct mife_setup_args_t {
    size_t secparam;
} mife_setup_args_t;

static void
mife_setup_args_init(mife_setup_args_t *args)
{
    args->secparam = 8;
}

static void
mife_setup_args_usage(void)
{
    printf("    --secparam λ       set security parameter to λ (default: 8)\n");
}

typedef struct mife_encrypt_args_t {
    size_t npowers;
    args_t *args;
} mife_encrypt_args_t;

static void
mife_encrypt_args_init(mife_encrypt_args_t *args)
{
    args->npowers = 8;
}

static void
mife_encrypt_args_usage(void)
{
    printf("    --npowers N        set the number of powers to N (default: 8)\n");
}

typedef struct mife_test_args_t {
    size_t secparam;
    size_t npowers;
} mife_test_args_t;

static void
mife_test_args_init(mife_test_args_t *args)
{
    args->npowers = 8;
    args->secparam = 8;
}

static void
mife_test_args_usage(void)
{
    printf("    --secparam λ       set security parameter to λ (default: 8)\n"
           "    --npowers N        set the number of powers to N (default: 8)\n");
}

typedef struct {
    size_t secparam;
    size_t npowers;
    enum scheme_e scheme;
} obf_obfuscate_args_t;

static void
obf_obfuscate_args_init(obf_obfuscate_args_t *args)
{
    args->secparam = 8;
    args->npowers = 8;
    args->scheme = SCHEME_LZ;
}

static void
obf_obfuscate_args_usage(void)
{
    printf("    --scheme S         set obfuscation scheme to S (options: LZ, MIFE | default: LZ)\n"
           "    --secparam λ       set security parameter to λ (default: 8)\n"
           "    --npowers N        set the number of powers to N (default: 8)\n");
}

typedef struct {
    size_t npowers;
    enum scheme_e scheme;
} obf_evaluate_args_t;

static void
obf_evaluate_args_init(obf_evaluate_args_t *args)
{
    args->npowers = 8;
    args->scheme = SCHEME_LZ;
}

static void
obf_evaluate_args_usage(void)
{
    printf("    --scheme S         set obfuscation scheme to S (options: LZ, MIFE | default: LZ)\n"
           "    --npowers N        set the number of powers to N (default: 8)\n");
}

typedef obf_obfuscate_args_t obf_test_args_t;
#define obf_test_args_init obf_obfuscate_args_init
#define obf_test_args_usage obf_obfuscate_args_usage
typedef obf_obfuscate_args_t obf_get_kappa_args_t;
#define obf_get_kappa_args_init obf_obfuscate_args_init
#define obf_get_kappa_args_usage obf_obfuscate_args_usage

static void
handle_options(int *argc, char ***argv, args_t *args, void *others,
               int (*other)(int *, char ***, void *),
               void (*f)(bool, int))
{
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-')
            break;

        if (!strcmp(cmd, "--secparam")) {
            if (*argc <= 1)
                f(false, EXIT_FAILURE);
            args->secparam = atoi((*argv)[1]);
            (*argv)++; (*argc)--;
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
                f(false, EXIT_FAILURE);
            }
            (*argv)++; (*argc)--;
        } else if (!strcmp(cmd, "--smart")) {
            args->smart = true;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (*argc <= 1)
                f(false, EXIT_FAILURE);
            args->nthreads = atoi((*argv)[1]);
            (*argv)++; (*argc)--;
        } else if (!strcmp(cmd, "--sigma")) {
            args->sigma = true;
        } else if (!strcmp(cmd, "--symlen")) {
            if (*argc <= 1)
                f(false, EXIT_FAILURE);
            args->symlen = atoi((*argv)[1]);
            (*argv)++; (*argc)--;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help")) {
            f(true, EXIT_SUCCESS);
        } else if (other) {
            if (other(argc, argv, others) == ERR)
                f(false, EXIT_FAILURE);
        } else {
            fprintf(stderr, "error: unknown argument '%s'\n", cmd);
            f(false, EXIT_FAILURE);
        }
        (*argv)++; (*argc)--;
    }
    if (*argc == 0) {
        fprintf(stderr, "error: missing circuit\n");
        f(false, EXIT_FAILURE);
    }
    args->circuit = (*argv)[0];
    {
        FILE *fp;
        void *res;

        if ((fp = fopen(args->circuit, "r")) == NULL) {
            fprintf(stderr, "error: opening circuit '%s' failed\n", args->circuit);
            exit(EXIT_FAILURE);
        }
        acirc_init(&args->circ);
        res = acirc_fread(&args->circ, fp);
        fclose(fp);
        if (res == NULL) {
            fprintf(stderr, "error: parsing circuit '%s' failed\n", args->circuit);
            exit(EXIT_FAILURE);
        }
    }
    const size_t consts = args->circ.consts.n ? 1 : 0;
    circ_params_init(&args->cp, args->circ.ninputs / args->symlen + consts, &args->circ);
    for (size_t i = 0; i < args->cp.n; ++i) {
        args->cp.ds[i] = args->symlen;
        args->cp.qs[i] = args->sigma ? args->symlen : (size_t) 1 << args->symlen;
    }
    if (consts) {
        args->cp.ds[args->cp.n - 1] = args->circ.consts.n;
        args->cp.qs[args->cp.n - 1] = 1;
    }

    (*argv)++; (*argc)--;
}

static void
mife_setup_usage(bool longform, int ret)
{
    printf("usage: mio mife setup [<args>] circuit\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        mife_setup_args_usage();
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
mife_setup_handle_options(int *argc, char ***argv, void *vargs)
{
    assert(*argc > 0);
    mife_setup_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--secparam")) {
        if (*argc <= 1)
            return ERR;
        args->secparam = atoi((*argv)[1]);
        (*argv)++; (*argc)--;
    } else {
        return ERR;
    }
    return OK;
}

static int
cmd_mife_setup(int argc, char **argv, args_t *args)
{
    mife_setup_args_t setup_args;
    mife_setup_args_init(&setup_args);

    argv++; argc--;
    handle_options(&argc, &argv, args, &setup_args, mife_setup_handle_options,
                   mife_setup_usage);
    if (mife_run_setup(args->vt, args->circuit, &args->cp, args->rng,
                       setup_args.secparam, NULL, args->nthreads) == ERR) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static void
mife_encrypt_usage(bool longform, int ret)
{
    printf("usage: mio mife encrypt [<args>] circuit input slot\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        mife_encrypt_args_usage();
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
mife_encrypt_handle_options(int *argc, char ***argv, void *vargs)
{
    assert(*argc > 0);

    mife_encrypt_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--npowers")) {
        if (*argc <= 1)
            return ERR;
        args->npowers = atoi((*argv)[1]);
        (*argv)++; (*argc)--;
    } else {
        return ERR;
    }
    return OK;
}

static int
cmd_mife_encrypt(int argc, char **argv, args_t *args)
{
    mife_encrypt_args_t encrypt_args;
    mife_encrypt_args_init(&encrypt_args);

    argv++; argc--;
    handle_options(&argc, &argv, args, &encrypt_args,
                   mife_encrypt_handle_options, mife_encrypt_usage);
    if (argc != 2) {
        switch (argc) {
        case 0:
            fprintf(stderr, "error: missing input\n");
            break;
        case 1:
            fprintf(stderr, "error: missing slot\n");
            break;
        default:
            fprintf(stderr, "error: too many arguments\n");
            break;
        }
        mife_encrypt_usage(false, EXIT_FAILURE);
    }
    int input[strlen(argv[0])];
    for (size_t i = 0; i < strlen(argv[0]); ++i) {
        input[i] = argv[0][i] - '0';
    }
    int slot = atoi(argv[1]);

    if (mife_run_encrypt(args->vt, args->circuit, &args->cp, args->rng, input, slot,
                         &encrypt_args.npowers, args->nthreads, NULL) == ERR) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static void
mife_decrypt_usage(bool longform, int ret)
{
    printf("usage: mio mife decrypt [<args>] circuit\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
cmd_mife_decrypt(int argc, char **argv, args_t *args)
{
    char *ek = NULL;
    char **cts = NULL;
    int *rop = NULL;
    size_t length, nslots = 0;
    int ret = EXIT_FAILURE;
    
    argv++; argc--;
    handle_options(&argc, &argv, args, NULL, NULL, mife_decrypt_usage);
    nslots = args->cp.n;

    length = snprintf(NULL, 0, "%s.ek\n", args->circuit);
    ek = my_calloc(length, sizeof ek[0]);
    snprintf(ek, length, "%s.ek\n", args->circuit);
    cts = my_calloc(nslots, sizeof cts[0]);
    for (size_t i = 0; i < nslots; ++i) {
        length = snprintf(NULL, 0, "%s.%lu.ct\n", args->circuit, i);
        cts[i] = my_calloc(length, sizeof cts[i][0]);
        (void) snprintf(cts[i], length, "%s.%lu.ct\n", args->circuit, i);
    }
    rop = my_calloc(args->cp.m, sizeof rop[0]);
    if (mife_run_decrypt(ek, cts, rop, args->vt, &args->cp, NULL, args->nthreads) == ERR) {
        fprintf(stderr, "error: mife decrypt failed\n");
        goto cleanup;
    }
    printf("result: ");
    for (size_t o = 0; o < args->cp.m; ++o) {
        printf("%d", rop[o]);
    }
    printf("\n");
    ret = EXIT_SUCCESS;
cleanup:
    if (ek)
        free(ek);
    if (cts) {
        for (size_t i = 0; i < nslots; ++i)
            free(cts[i]);
        free(cts);
    }
    if (rop)
        free(rop);
    return ret;
}

static void
mife_test_usage(bool longform, int ret)
{
    printf("usage: mio mife test [<args>] circuit\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        mife_test_args_usage();
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
        if (*argc <= 1)
            return ERR;
        args->npowers = atoi((*argv)[1]);
        (*argv)++; (*argc)--;
    } else if (!strcmp(cmd, "--secparam")) {
        if (*argc <= 1)
            return ERR;
        args->secparam = atoi((*argv)[1]);
        (*argv)++; (*argc)--;
    } else {
        return ERR;
    }
    return OK;

}

static int
cmd_mife_test(int argc, char **argv, args_t *args)
{
    size_t kappa = 0;
    mife_test_args_t test_args;
    mife_test_args_init(&test_args);

    argv++; argc--;
    handle_options(&argc, &argv, args, &test_args, mife_test_handle_options, mife_test_usage);
    if (args->smart) {
        kappa = mife_run_smart_kappa(args->circuit, &args->cp, &args->circ,
                                     test_args.npowers, args->nthreads, args->rng);
        if (kappa == 0)
            return EXIT_FAILURE;
    }
    if (mife_run_test(args->vt, args->circuit, &args->cp, args->rng, test_args.secparam,
                      &kappa, &test_args.npowers, args->nthreads) == ERR) {
        fprintf(stderr, "error: mife test failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static void
mife_get_kappa_usage(bool longform, int ret)
{
    printf("usage: mio mife get-kappa [<args>] circuit\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
cmd_mife_get_kappa(int argc, char **argv, args_t *args)
{
    const mmap_vtable *vt = &dummy_vtable;
    size_t kappa = 0;
    size_t npowers = 8;

    argv++, argc--;
    handle_options(&argc, &argv, args, NULL, NULL, mife_get_kappa_usage);
    if (args->smart) {
        kappa = mife_run_smart_kappa(args->circuit, &args->cp, &args->circ,
                                     npowers, args->nthreads, args->rng);
        if (kappa == 0)
            return EXIT_FAILURE;
    } else {
        if (mife_run_setup(vt, args->circuit, &args->cp, args->rng, 8, &kappa,
                           args->nthreads) == ERR) {
            fprintf(stderr, "error: mife setup failed\n");
            return EXIT_FAILURE;
        }
    }
    printf("κ = %lu\n", kappa);
    return EXIT_SUCCESS;
}

static void
mife_usage(bool longform, int ret)
{
    printf("usage: mio mife <command> [<args>]\n");
    if (longform) {
        printf("\nAvailable commands:\n\n"
               "   setup         run MIFE setup routine\n"
               "   encrypt       run MIFE encryption routine\n"
               "   decrypt       run MIFE decryption routine\n"
               "   test          run test suite\n"
               "   get-kappa     get κ value\n"
               "\n");
    }
    exit(ret);
}

static int
cmd_mife(int argc, char **argv)
{
    args_t args;
    int ret;
    
    if (argc == 1) {
        mife_usage(true, EXIT_FAILURE);
    }

    const char *const cmd = argv[1];
    args_init(&args);

    argv++; argc--;

    if (!strcmp(cmd, "setup")) {
        ret = cmd_mife_setup(argc, argv, &args);
    } else if (!strcmp(cmd, "encrypt")) {
        ret = cmd_mife_encrypt(argc, argv, &args);
    } else if (!strcmp(cmd, "decrypt")) {
        return cmd_mife_decrypt(argc, argv, &args);
    } else if (!strcmp(cmd, "test")) {
        return cmd_mife_test(argc, argv, &args);
    } else if (!strcmp(cmd, "get-kappa")) {
        return cmd_mife_get_kappa(argc, argv, &args);
    } else {
        fprintf(stderr, "error: unknown command '%s'\n", cmd);
        mife_usage(true, EXIT_FAILURE);
    }

    args_clear(&args);
    return ret;
}

/*******************************************************************************/

static int
obf_select_scheme(enum scheme_e scheme, acirc *circ, size_t npowers, bool sigma, size_t symlen,
                  obfuscator_vtable **vt, op_vtable **op_vt, obf_params_t **op)
{
    lz_obf_params_t lz_params;
    mife_obf_params_t mife_params;
    void *vparams;

    switch (scheme) {
    case SCHEME_LIN:
        fprintf(stderr, "LIN SCHEME BROKEN!\n");
        abort();
        /* vt = &lin_obfuscator_vtable; */
        /* op_vt = &lin_op_vtable; */
        /* lin_params.symlen = args->symlen; */
        /* lin_params.sigma = args->sigma; */
        /* vparams = &lin_params; */
        break;
    case SCHEME_LZ:
        *vt = &lz_obfuscator_vtable;
        *op_vt = &lz_op_vtable;
        lz_params.npowers = npowers;
        lz_params.symlen = symlen;
        lz_params.sigma = sigma;
        vparams = &lz_params;
        break;
    case SCHEME_MIFE:
        *vt = &mife_obfuscator_vtable;
        *op_vt = &mife_op_vtable;
        mife_params.npowers = npowers;
        mife_params.symlen = symlen;
        mife_params.sigma = sigma;
        vparams = &mife_params;
        break;
    }

    *op = (*op_vt)->new(circ, vparams);
    if (*op == NULL) {
        fprintf(stderr, "error: initializing obfuscation parameters failed\n");
        return ERR;
    }

    return OK;
}

static void
obf_obfuscate_usage(bool longform, int ret)
{
    printf("usage: mio obf obfuscate [<args>] circuit\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        obf_obfuscate_args_usage();
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
obf_obfuscate_handle_options(int *argc, char ***argv, void *vargs)
{
    assert(*argc > 0);
    obf_obfuscate_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--secparam")) {
        if (*argc <= 1)
            return ERR;
        args->secparam = atoi((*argv)[1]);
        (*argv)++; (*argc)--;
    } else if (!strcmp(cmd, "--npowers")) {
        if (*argc <= 1)
            return ERR;
        args->npowers = atoi((*argv)[1]);
        (*argv)++; (*argc)--;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1)
            return ERR;
        const char *scheme = (*argv)[1];
        if (!strcmp(scheme, "LZ")) {
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

static int
cmd_obf_obfuscate(int argc, char **argv, args_t *args)
{
    obf_obfuscate_args_t args_;
    obfuscator_vtable *vt;
    op_vtable *op_vt;
    obf_params_t *op;
    size_t kappa = 0;
    int ret = EXIT_SUCCESS;

    argv++; argc--;
    obf_obfuscate_args_init(&args_);
    handle_options(&argc, &argv, args, &args_, obf_obfuscate_handle_options,
                   obf_obfuscate_usage);
    if (obf_select_scheme(args_.scheme, &args->circ, args_.npowers,
                          args->sigma, args->symlen, &vt, &op_vt, &op) == ERR) {
        return EXIT_FAILURE;
    }
    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, &args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            return EXIT_FAILURE;
    }

    char fname[strlen(args->circuit) + sizeof ".obf\0"];
    snprintf(fname, sizeof fname, "%s.obf", args->circuit);
    if (obf_run_obfuscate(args->vt, vt, fname, op, args->secparam, &kappa,
                          args->nthreads, args->rng) == ERR) {
        ret = EXIT_FAILURE;
    }

    op_vt->free(op);

    return ret;
}

static void
obf_evaluate_usage(bool longform, int ret)
{
    printf("usage: mio obf evaluate [<args>] circuit input\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        obf_evaluate_args_usage();
        args_usage();
        printf("\n");
    }
    exit(ret);
}

static int
obf_evaluate_handle_options(int *argc, char ***argv, void *vargs)
{
    assert(*argc > 0);
    obf_evaluate_args_t *args = vargs;
    const char *cmd = (*argv)[0];
    if (!strcmp(cmd, "--npowers")) {
        if (*argc <= 1)
            return ERR;
        args->npowers = atoi((*argv)[1]);
        (*argv)++; (*argc)--;
    } else if (!strcmp(cmd, "--scheme")) {
        if (*argc <= 1)
            return ERR;
        const char *scheme = (*argv)[1];
        if (!strcmp(scheme, "LZ")) {
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
    
static int
cmd_obf_evaluate(int argc, char **argv, args_t *args)
{
    obf_evaluate_args_t evaluate_args;
    obfuscator_vtable *vt;
    op_vtable *op_vt;
    obf_params_t *op;

    argv++; argc--;
    obf_evaluate_args_init(&evaluate_args);
    handle_options(&argc, &argv, args, &evaluate_args,
                   obf_evaluate_handle_options, obf_evaluate_usage);
    if (argc != 1) {
        if (argc == 0) {
            fprintf(stderr, "error: missing input\n");
        } else {
            fprintf(stderr, "error: too many arguments\n");
        }
        obf_evaluate_usage(false, EXIT_FAILURE);
    }
    if (obf_select_scheme(evaluate_args.scheme, &args->circ, evaluate_args.npowers,
                          args->sigma, args->symlen, &vt, &op_vt, &op) == ERR) {
        return EXIT_FAILURE;
    }

    int input[strlen(argv[0])];
    for (size_t i = 0; i < strlen(argv[0]); ++i) {
        input[i] = argv[0][i] - '0';
    }
    int output[args->cp.m];
    char fname[strlen(args->circuit) + sizeof ".obf\0"];
    snprintf(fname, sizeof fname, "%s.obf", args->circuit);

    if (obf_run_evaluate(args->vt, vt, fname, op, input, output, args->nthreads,
                         NULL, NULL) == ERR)
        return ERR;
    printf("result: ");
    for (size_t i = 0; i < args->cp.m; ++i) {
        printf("%d", output[i]);
    }
    printf("\n");

    return OK;
}

static void
obf_test_usage(bool longform, int ret)
{
    printf("usage: mio obf test [<args>] circuit\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        obf_test_args_usage();
        args_usage();
        printf("\n");
    }
    exit(ret);
}

#define obf_test_handle_options obf_obfuscate_handle_options

static int
cmd_obf_test(int argc, char **argv, args_t *args)
{
    obf_test_args_t test_args;
    obfuscator_vtable *vt;
    op_vtable *op_vt;
    obf_params_t *op;
    size_t kappa = 0;
    int ret = EXIT_SUCCESS;

    argv++; argc--;
    obf_test_args_init(&test_args);
    handle_options(&argc, &argv, args, &test_args, obf_test_handle_options, obf_test_usage);
    if (obf_select_scheme(test_args.scheme, &args->circ, test_args.npowers,
                          args->sigma, args->symlen, &vt, &op_vt, &op) == ERR) {
        return EXIT_FAILURE;
    }
    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, &args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            return EXIT_FAILURE;
    }

    char fname[strlen(args->circuit) + sizeof ".obf\0"];
    snprintf(fname, sizeof fname, "%s.obf", args->circuit);
    if (obf_run_obfuscate(args->vt, vt, fname, op, args->secparam, &kappa,
                          args->nthreads, args->rng) == ERR) {
        return EXIT_FAILURE;
    }

    for (size_t t = 0; t < args->circ.tests.n; ++t) {
        int outp[args->cp.m];
        if (obf_run_evaluate(args->vt, vt, fname, op, args->circ.tests.inps[t],
                             outp, args->nthreads, &kappa, NULL) == ERR)
            return ERR;
        if (!print_test_output(t + 1, args->circ.tests.inps[t], args->circ.ninputs,
                               args->circ.tests.outs[t], outp, args->circ.outputs.n))
            ret = EXIT_FAILURE;
    }
    return ret;
}

static void
obf_get_kappa_usage(bool longform, int ret)
{
    printf("usage: mio obf get-kappa [<args>] circuit\n");
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        args_usage();
        printf("\n");
    }
    exit(ret);
}

#define obf_get_kappa_handle_options obf_obfuscate_handle_options

static int
cmd_obf_get_kappa(int argc, char **argv, args_t *args)
{
    obf_get_kappa_args_t args_;
    obfuscator_vtable *vt;
    op_vtable *op_vt;
    obf_params_t *op;
    size_t kappa = 0;
    size_t npowers = 8;

    argv++, argc--;
    obf_get_kappa_args_init(&args_);
    handle_options(&argc, &argv, args, &args_, obf_get_kappa_handle_options, obf_get_kappa_usage);
    if (obf_select_scheme(args_.scheme, &args->circ, npowers,
                          args->sigma, args->symlen, &vt, &op_vt, &op) == ERR) {
        return EXIT_FAILURE;
    }

    if (args->smart) {
        kappa = obf_run_smart_kappa(vt, &args->circ, op, args->nthreads, args->rng);
        if (kappa == 0)
            return EXIT_FAILURE;
    } else {
        if (obf_run_obfuscate(args->vt, vt, NULL, op, args->secparam, &kappa,
                              args->nthreads, args->rng) == ERR) {
            return EXIT_FAILURE;
        }
    }
    printf("κ = %lu\n", kappa);
    return EXIT_SUCCESS;

}

static void
obf_usage(bool longform, int ret)
{
    printf("usage: mio obf <command> [<args>]\n");
    if (longform) {
        printf("\nAvailable commands:\n\n"
               "   obfuscate    run circuit obfuscation\n"
               "   evaluate     run circuit evaluation\n"
               "   test         run test suite\n"
               "   get-kappa    get κ value\n"
               "\n");
    }
    exit(ret);
}

static int
cmd_obf(int argc, char **argv)
{
    args_t args;
    int ret = EXIT_FAILURE;

    if (argc == 1) {
        obf_usage(true, EXIT_FAILURE);
    }

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
    if (argc == 1) {
        usage(false, EXIT_FAILURE);
    }

    const char *const command = argv[1];

    argv++; argc--;

    if (!strcmp(command, "mife")) {
        return cmd_mife(argc, argv);
    } else if (!strcmp(command, "obf")) {
        return cmd_obf(argc, argv);
    } else if (!strcmp(command, "help")) {
        usage(true, EXIT_SUCCESS);
    } else if (!strcmp(command, "version")) {
        printf("%s %s\n", progname, progversion);
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "error: unknown command '%s'\n", command);
        usage(true, EXIT_FAILURE);
    }
}
