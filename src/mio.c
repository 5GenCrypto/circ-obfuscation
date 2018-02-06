#include "mmap.h"
#include "obfuscator.h"
#include "util.h"

#include "mife_run.h"
#include "obf_run.h"

#include "mife-cmr/mife.h"
#include "mife-gc/mife.h"
#include "obf-lz/obfuscator.h"
#include "obf-cmr/obfuscator.h"
/* #include "obf-polylog/obfuscator.h" */

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
#define WORDSIZE_DEFAULT 64
#define PADDING_DEFAULT 4
#define MMAP_DEFAULT_STR "DUMMY"

static int
args_get_mmap_scheme(const mmap_vtable **scheme, int *argc, char ***argv)
{
    if (*argc <= 1) return ERR;
    const char *mmap = (*argv)[1];
    if (!strcmp(mmap, "CLT")) {
        *scheme = &clt_vtable;
    } else if (!strcmp(mmap, "DUMMY")) {
        *scheme = &dummy_vtable;
    } else {
        fprintf(stderr, "%s: unknown mmap \"%s\"\n", errorstr, mmap);
        return ERR;
    }
    (*argv)++; (*argc)--;
    return OK;
}

typedef enum {
    OBF_SCHEME_LZ,
    OBF_SCHEME_CMR,
    /* OBF_SCHEME_POLYLOG, */
} obf_scheme_e;

#define OBF_SCHEME_DEFAULT     OBF_SCHEME_CMR
#define OBF_SCHEME_DEFAULT_STR "CMR"

static int
args_get_obf_scheme(obf_scheme_e *scheme, int *argc, char ***argv)
{
    if (*argc <= 1) return ERR;
    const char *str = (*argv)[1];
    if (!strcmp(str, "LZ")) {
        *scheme = OBF_SCHEME_LZ;
    } else if (!strcmp(str, "CMR")) {
        *scheme = OBF_SCHEME_CMR;
    /* } else if (!strcmp(str, "POLYLOG")) { */
    /*     *scheme = OBF_SCHEME_POLYLOG; */
    } else {
        fprintf(stderr, "%s: unknown obfuscation scheme '%s'\n", errorstr, str);
        return ERR;
    }
    (*argv)++; (*argc)--;
    return OK;
}

typedef enum {
    MIFE_SCHEME_CMR,
    MIFE_SCHEME_GC,
} mife_scheme_e;

#define MIFE_SCHEME_DEFAULT     MIFE_SCHEME_CMR
#define MIFE_SCHEME_DEFAULT_STR "CMR"

static int
args_get_mife_scheme(mife_scheme_e *scheme, int *argc, char ***argv)
{
    if (*argc <= 1) return ERR;
    const char *str = (*argv)[1];
    if (!strcmp(str, "CMR")) {
        *scheme = MIFE_SCHEME_CMR;
    } else if (!strcmp(str, "GC")) {
        *scheme = MIFE_SCHEME_GC;
    } else {
        fprintf(stderr, "%s: unknown mife scheme '%s'\n", errorstr, str);
        return ERR;
    }
    (*argv)++; (*argc)--;
    return OK;
}

typedef struct {
    char *circuit;
    acirc_t *circ;
    aes_randstate_t rng;
} args_t;

static void
args_init(args_t *args)
{
    args->circuit = NULL;
    args->circ = NULL;
    aes_randinit(args->rng);
}

static void
args_clear(args_t *args)
{
    if (args->circ)
        acirc_free(args->circ);
    if (args->rng)
        aes_randclear(args->rng);
}

static inline void usage_secparam(void)
{
    printf("    --secparam λ       set security parameter to λ (default: %d)\n",
           SECPARAM_DEFAULT);
}

static inline void usage_nthreads(void)
{
    printf("    --nthreads N       set the number of threads to N (default: %lu)\n",
           sysconf(_SC_NPROCESSORS_ONLN));
}

static inline void usage_npowers(void)
{
    printf("    --npowers N        set the number of powers to N (default: %d)\n",
           NPOWERS_DEFAULT);
}

static inline void usage_mife_scheme(void)
{
    printf("    --scheme S         set MIFE scheme to S (options: CMR, GC | default: %s)\n",
           MIFE_SCHEME_DEFAULT_STR);
}

static inline void usage_mife_ek(void)
{
    printf("    --ek F             save evaluation key to file F (default: <circuit>.ek)\n");
}

static inline void usage_mife_sk(void)
{
    printf("    --sk F             save secret key to file F (default: <circuit>.sk)\n");
}

static inline void usage_mife_gc_padding(void)
{
    printf("    --padding N        set padding to N (default: %d)\n",
           PADDING_DEFAULT);
}

static inline void usage_mife_gc_wirelen(void)
{
    printf("    --wirelen N        set wire length to N (default: %d)\n",
           SECPARAM_DEFAULT);
}

static inline void usage_obf_scheme(void)
{
    printf("    --scheme S         set obfuscation scheme to S (options: CMR, LZ, POLYLOG | default: %s)\n",
           OBF_SCHEME_DEFAULT_STR);
}

static inline void usage_mmap()
{
    printf("    --mmap M           set mmap to M (options: CLT, DUMMY | default: %s)\n",
           MMAP_DEFAULT_STR);
}

static inline void usage_saved()
{
    printf("    --saved            use saved encodings\n");
}

static inline void usage_verbose(void)
{
    printf("    --verbose          be verbose\n");
}

static inline void usage_help(void)
{
    printf("    --help             print this message and exit\n");
}

static int
args_get_size_t(size_t *result, int *argc, char ***argv)
{
    char *endptr;
    if (*argc <= 1) return ERR;
    *result = strtol((*argv)[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "%s: invalid argument '%s'\n", errorstr, (*argv)[1]);
        return ERR;
    }
    (*argv)++; (*argc)--;
    return OK;
}

static int
args_get_string(char **result, int *argc, char ***argv)
{
    if (*argc <= 1) return ERR;
    *result = (*argv)[1];
    (*argv)++; (*argc)--;
    return OK;
}

typedef struct {
    size_t secparam;
    mife_scheme_e scheme;
    const mmap_vtable *mmap;
    size_t nthreads;
    size_t npowers;
    char *ek;
    char *sk;
    size_t padding;
    size_t wirelen;
} mife_setup_args_t;

static void
mife_setup_args_init(mife_setup_args_t *args)
{
    args->secparam = SECPARAM_DEFAULT;
    args->scheme = MIFE_SCHEME_DEFAULT;
    args->mmap = &dummy_vtable;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->npowers = NPOWERS_DEFAULT;
    args->ek = NULL;
    args->sk = NULL;
    args->padding = PADDING_DEFAULT;
    args->wirelen = SECPARAM_DEFAULT;
}

static void
mife_setup_usage(bool longform, int ret)
{
    printf("usage: %s mife setup [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        usage_secparam();
        usage_mife_scheme();
        usage_mmap();
        usage_nthreads();
        usage_npowers();
        usage_mife_ek();
        usage_mife_sk();
        usage_verbose();
        usage_help();
        printf("\n  GC-specific arguments:\n");
        usage_mife_gc_padding();
        usage_mife_gc_wirelen();
        printf("\n");
    }
    exit(ret);
}

static int
mife_setup_handle_options(int *argc, char ***argv, void *vargs)
{
    mife_setup_args_t *args = vargs;
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-') break;
        if (!strcmp(cmd, "--secparam")) {
            if (args_get_size_t(&args->secparam, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--scheme")) {
            if (args_get_mife_scheme(&args->scheme, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--mmap")) {
            if (args_get_mmap_scheme(&args->mmap, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--npowers")) {
            if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--ek")) {
            if (args_get_string(&args->ek, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--sk")) {
            if (args_get_string(&args->sk, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--padding")) {
            if (args_get_size_t(&args->padding, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--wirelen")) {
            if (args_get_size_t(&args->wirelen, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            mife_setup_usage(true, EXIT_SUCCESS);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd); return ERR;
        }
        (*argv)++; (*argc)--;
    }
    return OK;
}

typedef struct {
    mife_scheme_e scheme;
    const mmap_vtable *mmap;
    size_t nthreads;
} mife_encrypt_args_t;

static void
mife_encrypt_args_init(mife_encrypt_args_t *args)
{
    args->scheme = MIFE_SCHEME_DEFAULT;
    args->mmap = &dummy_vtable;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
}

static void
mife_encrypt_usage(bool longform, int ret)
{
    printf("usage: %s mife encrypt [<args>] circuit input slot\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        usage_mife_scheme();
        usage_mmap();
        usage_nthreads();
        printf("\n");
    }
    exit(ret);
}

static int
mife_encrypt_handle_options(int *argc, char ***argv, void *vargs)
{
    mife_encrypt_args_t *args = vargs;
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-') break;
        if (!strcmp(cmd, "--scheme")) {
            if (args_get_mife_scheme(&args->scheme, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            mife_encrypt_usage(true, EXIT_SUCCESS);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd); return ERR;
        }
        (*argv)++; (*argc)--;
    }
    return OK;
}

typedef struct {
    mife_scheme_e scheme;
    const mmap_vtable *mmap;
    size_t nthreads;
    bool saved;
} mife_decrypt_args_t;

static void
mife_decrypt_args_init(mife_decrypt_args_t *args)
{
    args->scheme = MIFE_SCHEME_DEFAULT;
    args->mmap = &dummy_vtable;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->saved = false;
}

static void
mife_decrypt_usage(bool longform, int ret)
{
    printf("usage: %s mife decrypt [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        usage_mife_scheme();
        usage_mmap();
        usage_nthreads();
        usage_saved();
        usage_verbose();
        usage_help();
        printf("\n");
    }
    exit(ret);
}

static int
mife_decrypt_handle_options(int *argc, char ***argv, void *vargs)
{
    mife_decrypt_args_t *args = vargs;
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-') break;
        if (!strcmp(cmd, "--scheme")) {
            if (args_get_mife_scheme(&args->scheme, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--mmap")) {
                if (args_get_mmap_scheme(&args->mmap, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--saved")) {
            args->saved = true;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            mife_decrypt_usage(true, EXIT_SUCCESS);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd); return ERR;
        }
        (*argv)++; (*argc)--;
    }
    return OK;
}

typedef struct {
    size_t secparam;
    mife_scheme_e scheme;
    const mmap_vtable *mmap;
    size_t npowers;
    size_t nthreads;
    size_t padding;
    size_t wirelen;
} mife_test_args_t;

static void
mife_test_args_init(mife_test_args_t *args)
{
    args->secparam = SECPARAM_DEFAULT;
    args->scheme = MIFE_SCHEME_DEFAULT;
    args->mmap = &dummy_vtable;
    args->npowers = NPOWERS_DEFAULT;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->padding = PADDING_DEFAULT;
    args->wirelen = SECPARAM_DEFAULT;
}

static void
mife_test_usage(bool longform, int ret)
{
    printf("usage: %s mife test [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        usage_secparam();
        usage_mife_scheme();
        usage_mmap();
        usage_nthreads();
        usage_npowers();
        usage_verbose();
        usage_help();
        printf("\n  GC-specific arguments:\n");
        usage_mife_gc_padding();
        usage_mife_gc_wirelen();
        printf("\n");
    }
    exit(ret);
}

static int
mife_test_handle_options(int *argc, char ***argv, void *vargs)
{
    mife_test_args_t *args = vargs;
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-') break;
        if (!strcmp(cmd, "--secparam")) {
            if (args_get_size_t(&args->secparam, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--scheme")) {
            if (args_get_mife_scheme(&args->scheme, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--mmap")) {
            if (args_get_mmap_scheme(&args->mmap, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--npowers")) {
            if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--padding")) {
            if (args_get_size_t(&args->padding, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--wirelen")) {
            if (args_get_size_t(&args->padding, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            mife_test_usage(true, EXIT_SUCCESS);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd); return ERR;
        }
        (*argv)++; (*argc)--;
    }
    return OK;
}

typedef struct {
    mife_scheme_e scheme;
} mife_get_kappa_args_t;

static void
mife_get_kappa_args_init(mife_get_kappa_args_t *args)
{
    args->scheme = MIFE_SCHEME_DEFAULT;
}

static void
mife_get_kappa_usage(bool longform, int ret)
{
    printf("usage: %s mife get-kappa [<args>] circuit\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        usage_mife_scheme();
        usage_verbose();
        usage_help();
        printf("\n");
    }
    exit(ret);
}

static int
mife_get_kappa_handle_options(int *argc, char ***argv, void *vargs)
{
    mife_get_kappa_args_t *args = vargs;
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-')
            break;
        if (!strcmp(cmd, "--scheme")) {
            if (args_get_mife_scheme(&args->scheme, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            mife_get_kappa_usage(true, EXIT_SUCCESS);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd); return ERR;
        }
        (*argv)++; (*argc)--;
    }
    return OK;
}

typedef struct {
    size_t secparam;
    obf_scheme_e scheme;
    const mmap_vtable *mmap;
    size_t nthreads;
    size_t npowers;
} obf_obfuscate_args_t;

static void
obf_obfuscate_args_init(obf_obfuscate_args_t *args)
{
    args->secparam = SECPARAM_DEFAULT;
    args->scheme = OBF_SCHEME_CMR;
    args->mmap = &dummy_vtable;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->npowers = NPOWERS_DEFAULT;
}

static void
obf_obfuscate_or_test_usage(bool longform, int ret, const char *cmd)
{
    printf("usage: %s obf %s [<args>] circuit\n", progname, cmd);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        usage_secparam();
        usage_obf_scheme();
        usage_mmap();
        usage_nthreads();
        usage_npowers();
        usage_verbose();
        usage_help();
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
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-') break;
        if (!strcmp(cmd, "--secparam")) {
            if (args_get_size_t(&args->secparam, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--scheme")) {
            if (args_get_obf_scheme(&args->scheme, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--mmap")) {
            if (args_get_mmap_scheme(&args->mmap, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--npowers")) {
            if (args_get_size_t(&args->npowers, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            obf_obfuscate_usage(true, EXIT_SUCCESS);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd); return ERR;
        }
        (*argv)++; (*argc)--;
    }
    return OK;
}

typedef struct {
    obf_scheme_e scheme;
    const mmap_vtable *mmap;
    size_t nthreads;
} obf_evaluate_args_t;

static void
obf_evaluate_args_init(obf_evaluate_args_t *args)
{
    args->scheme = OBF_SCHEME_CMR;
    args->mmap = &dummy_vtable;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
}

static void
obf_evaluate_usage(bool longform, int ret)
{
    printf("usage: %s obf evaluate [<args>] circuit input\n", progname);
    if (longform) {
        printf("\nAvailable arguments:\n\n");
        usage_obf_scheme();
        usage_mmap();
        usage_nthreads();
        usage_verbose();
        usage_help();
        printf("\n");
    }
    exit(ret);
}

static int
obf_evaluate_handle_options(int *argc, char ***argv, void *vargs)
{
    obf_evaluate_args_t *args = vargs;
    while (*argc > 0) {
        const char *cmd = (*argv)[0];
        if (cmd[0] != '-') break;
        if (!strcmp(cmd, "--scheme")) {
            if (args_get_obf_scheme(&args->scheme, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--mmap")) {
                if (args_get_mmap_scheme(&args->mmap, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--nthreads")) {
            if (args_get_size_t(&args->nthreads, argc, argv) == ERR) return ERR;
        } else if (!strcmp(cmd, "--verbose")) {
            g_verbose = true;
        } else if (!strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
            obf_evaluate_usage(true, EXIT_SUCCESS);
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n", errorstr, cmd); return ERR;
        }
        (*argv)++; (*argc)--;
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
        usage_obf_scheme();
        usage_verbose();
        usage_help();
        printf("\n");
    }
    exit(ret);
}

#define obf_get_kappa_handle_options obf_evaluate_handle_options

static void
handle_options(int *argc, char ***argv, int left, args_t *args, void *others,
               int (*handle)(int *, char ***, void *),
               void (*usage)(bool, int))
{
    if (handle(argc, argv, others) == ERR)
        usage(true, EXIT_FAILURE);
    if (*argc == 0) {
        fprintf(stderr, "%s: missing circuit\n", errorstr);
        usage(false, EXIT_FAILURE);
    } else if (*argc < left + 1) {
        fprintf(stderr, "%s: too few arguments\n", errorstr);
        usage(false, EXIT_FAILURE);
    } else if (*argc > left + 1) {
        fprintf(stderr, "%s: too many arguments\n", errorstr);
        usage(false, EXIT_FAILURE);
    }
    args->circuit = (*argv)[0];
    args->circ = acirc_new(args->circuit, false, true);
    if (args->circ == NULL) {
        fprintf(stderr, "%s: parsing circuit '%s' failed\n", errorstr, args->circuit);
        exit(EXIT_FAILURE);
    }
    (*argv)++; (*argc)--;
}

static int
mife_select_scheme(mife_scheme_e scheme, acirc_t *circ, size_t npowers,
                   size_t padding, size_t wirelen, mife_vtable **vt,
                   op_vtable **op_vt, obf_params_t **op)
{
    mife_cmr_params_t mife_cmr_params;
    mife_gc_params_t mife_gc_params;
    void *vparams = NULL;
    switch (scheme) {
    case MIFE_SCHEME_CMR:
        *vt = &mife_cmr_vtable;
        *op_vt = &mife_cmr_op_vtable;
        mife_cmr_params.npowers = npowers;
        vparams = &mife_cmr_params;
        break;
    case MIFE_SCHEME_GC:
        *vt = &mife_gc_vtable;
        *op_vt = &mife_gc_op_vtable;
        mife_gc_params.padding = padding;
        mife_gc_params.wirelen = wirelen;
        vparams = &mife_gc_params;
        break;
    }
    *op = (*op_vt)->new(circ, vparams);
    if (*op == NULL) {
        fprintf(stderr, "%s: initializing MIFE parameters failed\n", errorstr);
        return ERR;
    }
    if (g_verbose) {
        if (circ_params_print(circ) == ERR)
            return ERR;
        if ((*op_vt)->print)
            (*op_vt)->print(*op);
    }
    return OK;
}

static int
cmd_mife_setup(int argc, char **argv, args_t *args)
{
    mife_setup_args_t args_;
    mife_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    int ret = ERR;

    argv++; argc--;
    mife_setup_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, mife_setup_handle_options, mife_setup_usage);
    if (mife_select_scheme(args_.scheme, args->circ, args_.npowers, args_.padding,
                           args_.wirelen, &vt, &op_vt, &op) == ERR)
        goto cleanup;
    if (mife_run_setup(args_.mmap, vt, args->circ, op, args_.secparam, args_.ek,
                       args_.sk, NULL, args_.nthreads, args->rng) == ERR)
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
    mife_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    long *input = NULL;
    size_t ninputs;
    size_t slot;
    int ret = ERR;

    argv++; argc--;
    mife_encrypt_args_init(&args_);
    handle_options(&argc, &argv, 2, args, &args_, mife_encrypt_handle_options, mife_encrypt_usage);
    ninputs = strlen(argv[0]);
    if ((input = my_calloc(ninputs, sizeof input[0])) == NULL)
        goto cleanup;
    for (size_t i = 0; i < ninputs; ++i) {
        if ((input[i] = char_to_long(argv[0][i])) < 0)
            goto cleanup;
    }
    if (args_get_size_t(&slot, &argc, &argv) == ERR)
        goto cleanup;
    if (mife_select_scheme(args_.scheme, args->circ, 0, 0, 0, &vt, &op_vt, &op) == ERR)
        goto cleanup;
    if (mife_run_encrypt(args_.mmap, vt, args->circ, op, input, ninputs, slot,
                         args_.nthreads, NULL, args->rng) == ERR)
        goto cleanup;
    ret = OK;
cleanup:
    if (input)
        free(input);
    if (op)
        op_vt->free(op);
    return ret;
}

static int
cmd_mife_decrypt(int argc, char **argv, args_t *args)
{
    mife_decrypt_args_t args_;
    mife_vtable *vt = NULL;
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
    if (args_.saved)
        acirc_set_saved(args->circ);
    if (mife_select_scheme(args_.scheme, args->circ, 0, 0, 0, &vt, &op_vt, &op) == ERR)
        goto cleanup;
    nslots = acirc_nslots(args->circ);

    length = snprintf(NULL, 0, "%s.ek\n", args->circuit);
    ek = my_calloc(length, sizeof ek[0]);
    snprintf(ek, length, "%s.ek\n", args->circuit);
    cts = my_calloc(nslots, sizeof cts[0]);
    for (size_t i = 0; i < nslots; ++i) {
        length = snprintf(NULL, 0, "%s.%lu.ct\n", args->circuit, i);
        cts[i] = my_calloc(length, sizeof cts[i][0]);
        (void) snprintf(cts[i], length, "%s.%lu.ct\n", args->circuit, i);
    }
    rop = my_calloc(acirc_noutputs(args->circ), sizeof rop[0]);
    if (mife_run_decrypt(args_.mmap, vt, args->circ, ek, cts, rop, op, NULL,
                         args_.nthreads) == ERR) {
        fprintf(stderr, "%s: mife decrypt failed\n", errorstr);
        goto cleanup;
    }
    printf("result: ");
    for (size_t o = 0; o < acirc_noutputs(args->circ); ++o) {
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
    mife_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    size_t kappa = 0;
    int ret = ERR;

    argv++; argc--;
    mife_test_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, mife_test_handle_options,
                   mife_test_usage);
    if (mife_select_scheme(args_.scheme, args->circ, args_.npowers,
                           args_.padding, args_.wirelen, &vt, &op_vt,
                           &op) == ERR)
        goto cleanup;
    /* if (args->smart) { */
    /*     kappa = mife_run_smart_kappa(vt, args->circ, op, args_.nthreads, */
    /*                                  args->rng); */
    /*     if (kappa == 0) */
    /*         goto cleanup; */
    /* } */
    if (mife_run_test(args_.mmap, vt, args->circ, op, args_.secparam,
                      &kappa, args_.nthreads, args->rng) == ERR) {
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
    mife_vtable *vt = NULL;
    op_vtable *op_vt = NULL;
    obf_params_t *op = NULL;
    size_t kappa = 0;
    int ret = ERR;

    argv++; argc--;
    mife_get_kappa_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, mife_get_kappa_handle_options,
                   mife_get_kappa_usage);
    if (mife_select_scheme(args_.scheme, args->circ, 1, 0, 0, &vt, &op_vt,
                           &op) == ERR)
        goto cleanup;
    /* if (args->smart) { */
    /*     kappa = mife_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng); */
    /*     if (kappa == 0) */
    /*         goto cleanup; */
    /* } else { */
        if (mife_run_setup(&dummy_vtable, vt, args->circ, op, 8, NULL, NULL, &kappa,
                           1, args->rng) == ERR) {
            fprintf(stderr, "%s: mife setup failed\n", errorstr);
            goto cleanup;
        }
    /* } */
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
                  size_t wordsize, obfuscator_vtable **vt,
                  op_vtable **op_vt, obf_params_t **op)
{
    (void) wordsize;
    lz_obf_params_t lz_params;
    mobf_obf_params_t mobf_params;
    /* polylog_obf_params_t polylog_params; */
    void *vparams = NULL;

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
    /* case OBF_SCHEME_POLYLOG: */
    /*     *vt = &polylog_obfuscator_vtable; */
    /*     *op_vt = &polylog_op_vtable; */
    /*     polylog_params.wordsize = wordsize; */
    /*     vparams = &polylog_params; */
    /*     break; */
    }
    *op = (*op_vt)->new(circ, vparams);
    if (*op == NULL) {
        fprintf(stderr, "%s: initializing obfuscation parameters failed\n", errorstr);
        return ERR;
    }
    if (g_verbose) {
        if (circ_params_print(circ) == ERR)
            return ERR;
        if ((*op_vt)->print)
            (*op_vt)->print(*op);
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
    size_t kappa = 0;
    int ret = ERR;

    argv++; argc--;
    obf_obfuscate_args_init(&args_);
    handle_options(&argc, &argv, 0, args, &args_, obf_obfuscate_handle_options,
                   obf_obfuscate_usage);
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers, 0,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;

    fname = makestr("%s.obf", args->circuit);
    /* if (args_.scheme == OBF_SCHEME_POLYLOG && args->vt == &clt_vtable) */
    /*     args->vt = &clt_pl_vtable; */
    /* if (args->smart) { */
    /*     kappa = obf_run_smart_kappa(vt, args->circ, op, args->nthreads, args->rng); */
    /*     if (kappa == 0) */
    /*         goto cleanup; */
    /* } */
    if (obf_run_obfuscate(args_.mmap, vt, fname, op, args_.secparam, &kappa,
                          args_.nthreads, args->rng) == ERR)
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
    if (obf_select_scheme(args_.scheme, args->circ, 1, 0,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;

    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    if ((fname = my_calloc(length, sizeof fname[0])) == NULL)
        goto cleanup;
    snprintf(fname, length, "%s.obf", args->circuit);

    /* if (args_.scheme == OBF_SCHEME_POLYLOG && args->vt == &clt_vtable) */
    /*     args->vt = &clt_pl_vtable; */
    if ((input = my_calloc(strlen(argv[0]), sizeof input[0])) == NULL)
        goto cleanup;
    if ((output = my_calloc(acirc_noutputs(args->circ), sizeof output[0])) == NULL)
        goto cleanup;

    for (size_t i = 0; i < strlen(argv[0]); ++i) {
        if ((input[i] = char_to_long(argv[0][i])) < 0)
            goto cleanup;
    }
    if (obf_run_evaluate(args_.mmap, vt, fname, op, input, strlen(argv[0]), output,
                         acirc_noutputs(args->circ), args_.nthreads, NULL, NULL) == ERR)
        goto cleanup;

    printf("result: ");
    for (size_t i = 0; i < acirc_noutputs(args->circ); ++i)
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
    if (obf_select_scheme(args_.scheme, args->circ, args_.npowers, 0,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;
    /* if (args_.scheme == OBF_SCHEME_POLYLOG && args->vt == &clt_vtable) */
    /*     args->vt = &clt_pl_vtable; */
    /* if (args->smart) { */
    /*     kappa = obf_run_smart_kappa(vt, args->circ, op, args_.nthreads, args->rng); */
    /*     if (kappa == 0) */
    /*         goto cleanup; */
    /* } */

    length = snprintf(NULL, 0, "%s.obf\n", args->circuit);
    if ((fname = my_calloc(length, sizeof fname[0])) == NULL)
        goto cleanup;
    snprintf(fname, length, "%s.obf", args->circuit);
    if (obf_run_obfuscate(args_.mmap, vt, fname, op, args_.secparam, &kappa,
                          args_.nthreads, args->rng) == ERR)
        goto cleanup;

    for (size_t t = 0; t < acirc_ntests(args->circ); ++t) {
        long outp[acirc_noutputs(args->circ)];
        if (obf_run_evaluate(args_.mmap, vt, fname, op, acirc_test_input(args->circ, t),
                             acirc_ninputs(args->circ), outp, acirc_noutputs(args->circ),
                             args_.nthreads, &kappa, NULL) == ERR)
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
    handle_options(&argc, &argv, 0, args, &args_,
                   obf_get_kappa_handle_options, obf_get_kappa_usage);
    if (obf_select_scheme(args_.scheme, args->circ, 1, 0,
                          &vt, &op_vt, &op) == ERR)
        goto cleanup;

    /* if (args_.smart) { */
    /*     kappa = obf_run_smart_kappa(vt, args->circ, op, 1, args->rng); */
    /*     if (kappa == 0) */
    /*         goto cleanup; */
    /* } else { */
        if (obf_run_obfuscate(&dummy_vtable, vt, NULL, op, 8, NULL,
                              1, args->rng) == ERR)
            goto cleanup;
    /* } */
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
        /* printf("· GC:      Garbled-circuit-based scheme\n"); */
        printf("\n");
        printf("Supported obfuscation schemes:\n");
        printf("· CMR:     CMR obfuscation scheme [http://ia.cr/2017/826, §5.4]\n");
        printf("· LZ:      Linnerman scheme       [http://ia.cr/2017/826, §B]\n");
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
