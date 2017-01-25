#include "mmap.h"
#include "obfuscator.h"
#include "util.h"

#include "lin/obfuscator.h"
#include "lz/obfuscator.h"

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

enum scheme_e {
    SCHEME_LIN,
    SCHEME_LZ,
};

enum mmap_e {
    MMAP_CLT,
    MMAP_DUMMY,
};

static char *progname = "circobf";

static char *
scheme_to_string(enum scheme_e scheme)
{
    switch (scheme) {
    case SCHEME_LIN:
        return "LIN";
    case SCHEME_LZ:
        return "LZ";
    }
    abort();
}

static char *
mmap_to_string(enum mmap_e mmap)
{
    switch (mmap) {
    case MMAP_CLT:
        return "CLT";
    case MMAP_DUMMY:
        return "DUMMY";
    }
    abort();
}

struct args_t {
    char *circuit;
    enum mmap_e mmap;
    size_t secparam;
    size_t kappa;
    size_t nthreads;
    bool evaluate;
    bool obfuscate;
    bool dry_run;
    enum scheme_e scheme;
    /* LIN/LZ specific flags */
    size_t symlen;
    bool sigma;
    /* LZ specific flags */
    size_t npowers;
    /* Helper flags */
    bool get_kappa;

};

static void
args_init(struct args_t *args)
{
    args->circuit = NULL;
    args->mmap = MMAP_DUMMY;
    args->secparam = 16;
    args->kappa = 0;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->evaluate = true;
    args->obfuscate = true;
    args->dry_run = false;
    args->scheme = SCHEME_LZ;
    /* LIN/LZ specific flags */
    args->symlen = 1;
    args->sigma = false;
    /* LZ specific flags */
    args->npowers = 8;
    /* Helper flags */
    args->get_kappa = false;
}

static void
args_print(const struct args_t *args)
{
    fprintf(stderr, "Obfuscation details:\n"
            "* Circuit: %s\n"
            "* Multilinear map: %s\n"
            "* Security parameter: %lu\n"
            "* Obfuscating? %s Evaluating? %s\n"
            "* Scheme: %s\n"
            "* # threads: %lu\n"
            ,
            args->circuit, mmap_to_string(args->mmap), args->secparam,
            args->obfuscate ? "Y" : "N",
            args->evaluate ? "Y" : "N", scheme_to_string(args->scheme),
            args->nthreads);
}

static void
usage(int ret)
{
    struct args_t defaults;
    args_init(&defaults);
    printf("Usage: %s [options] <circuit>\n\n", progname);
    printf("Options:\n"
"    --dry-run         don't obfuscate/evaluate\n"
"    --debug <LEVEL>   set debug level (options: ERROR, WARN, DEBUG, INFO | default: ERROR)\n"
"    --evaluate, -e    evaluate obfuscation (default: %s)\n"
"    --obfuscate, -o   construct obfuscation (default: %s)\n"
"    --kappa, -k <κ>   set kappa to κ when obfuscating (default: guess)\n"
"    --lambda, -l <λ>  set security parameter to λ when obfuscating (default: %lu)\n"
"    --nthreads <N>    set the number of threads to N (default: %lu)\n"
"    --scheme <NAME>   set scheme to NAME (options: LIN, LZ | default: %s)\n"
"    --mmap <NAME>     set mmap to NAME (options: CLT, DUMMY | default: %s)\n"
"\n"
"  LIN/LZ specific flags:\n"
"    --symlen          symbol length, in bits (default: %lu)\n"
"    --sigma           use sigma vectors (default: %s)\n"
"\n"
"  LZ specific flags:\n"
"    --npowers <N>     use N powers (default: %lu)\n"
"\n"
"  Helper flags:\n"
"    --get-kappa       determine the correct κ value for the given circuit\n"
"    --verbose, -v     be verbose\n"
"    --help, -h        print this message\n"
           ,
           defaults.evaluate ? "yes" : "no",
           defaults.obfuscate ? "yes" : "no",
           defaults.secparam, defaults.nthreads,
           scheme_to_string(defaults.scheme), mmap_to_string(defaults.mmap),
           defaults.symlen, defaults.sigma ? "yes" : "no",
           defaults.npowers);
    exit(ret);
}

static const struct option opts[] = {
    {"debug", required_argument, 0, 'D'},
    {"dry-run", no_argument, 0, 'd'},
    {"evaluate", no_argument, 0, 'e'},
    {"obfuscate", no_argument, 0, 'o'},
    {"kappa", required_argument, 0, 'k'},
    {"lambda", required_argument, 0, 'l'},
    {"nthreads", required_argument, 0, 't'},
    {"mmap", required_argument, 0, 'M'},
    {"scheme", required_argument, 0, 'S'},
    /* LIN/LZ specific settings */
    {"sigma", no_argument, 0, 's'},
    {"symlen", required_argument, 0, 'L'},
    /* LZ specific settings */
    {"npowers", required_argument, 0, 'n'},
    /* Helper flags */
    {"get-kappa", no_argument, 0, 'g'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};
static const char *short_opts = "adD:egk:lL:M:n:osS:t:vh";

static int
_evaluate(const obfuscator_vtable *vt, const struct args_t *args,
          const acirc *c, const obfuscation *obf)
{
    int res[c->outputs.n];
    int ret = OK;
    double start, end;
    unsigned int degree;

    for (size_t i = 0; i < c->tests.n; i++) {
        start = current_time();
        if (vt->evaluate(res, c->tests.inps[i], obf, args->nthreads, &degree) == ERR)
            return ERR;
        end = current_time();
        if (g_verbose)
            fprintf(stderr, "evaluation: %.2fs\n", end - start);

        if (args->get_kappa) {
            printf("κ = %u\n", degree);
            break;
        }

        bool ok = true;
        for (size_t j = 0; j < c->outputs.n; ++j) {
            switch (args->scheme) {
            case SCHEME_LZ:
                if (!!res[j] != !!c->tests.outs[i][j]) {
                    ok = false;
                    ret = ERR;
                }
                break;
            case SCHEME_LIN:
                if (res[j] == (c->tests.outs[i][j] != 1)) {
                    ok = false;
                    ret = ERR;
                }
                break;
            }
        }
        if (!ok)
            printf("\033[1;41m");
        printf("test %lu input=", i);
        array_printstring_rev(c->tests.inps[i], c->ninputs);
        printf(" expected=");
        array_printstring_rev(c->tests.outs[i], c->outputs.n);
        printf(" got=");
        array_printstring_rev(res, c->outputs.n);
        if (!ok)
            printf("\033[0m");
        puts("");
    }
    return ret;
}

static int
run(const struct args_t *args)
{
    obf_params_t *params;
    acirc c;
    const mmap_vtable *mmap;
    const obfuscator_vtable *vt;
    const op_vtable *op_vt;
    lin_obf_params_t lin_params;
    lz_obf_params_t lz_params;
    void *vparams;

    switch (args->mmap) {
    case MMAP_CLT:
        mmap = &clt_vtable;
        break;
    case MMAP_DUMMY:
        mmap = &dummy_vtable;
        break;
    default:
        abort();
    }

    acirc_verbose(g_verbose);
    {
        FILE *fp = fopen(args->circuit, "r");
        if (acirc_fread(&c, fp) == NULL) {
            fclose(fp);
            fprintf(stderr, "error: parsing circuit '%s' failed\n", args->circuit);
            exit(EXIT_FAILURE);
        }
        fclose(fp);
    }

    if (g_verbose) {
        printf("Circuit info:\n");
        printf("* ninputs: %lu\n", c.ninputs);
        printf("* nconsts: %lu\n", c.consts.n);
        printf("* noutputs: %lu\n", c.outputs.n);
        printf("* size: %lu\n", c.gates.n);
        printf("* nmuls: %lu\n", acirc_nmuls(&c));
        printf("* depth: %lu\n", acirc_max_depth(&c));
        printf("* degree: %lu\n", acirc_max_degree(&c));
        printf("* delta: %lu\n", acirc_delta(&c));
    }

    switch (args->scheme) {
    case SCHEME_LIN:
        vt = &lin_obfuscator_vtable;
        op_vt = &lin_op_vtable;
        lin_params.symlen = args->symlen;
        lin_params.sigma = args->sigma;
        vparams = &lin_params;
        break;
    case SCHEME_LZ:
        vt = &lz_obfuscator_vtable;
        op_vt = &lz_op_vtable;
        lz_params.npowers = args->npowers;
        lz_params.symlen = args->symlen;
        lz_params.sigma = args->sigma;
        vparams = &lz_params;
        break;
    }

    params = op_vt->new(&c, vparams);
    if (params == NULL) {
        fprintf(stderr, "error: initialize obfuscator parameters failed\n");
        exit(EXIT_FAILURE);
    }

    if (args->dry_run) {
        obfuscation *obf;
        obf = vt->new(mmap, params, args->secparam, args->kappa);
        if (obf == NULL) {
            fprintf(stderr, "error: initializing obfuscator failed\n");
            exit(EXIT_FAILURE);
        }
        vt->free(obf);
        goto cleanup;
    }

    if (args->obfuscate) {
        char fname[strlen(args->circuit) + 5];
        obfuscation *obf;
        double start, end;
        FILE *f;

        if (g_verbose)
            fprintf(stderr, "obfuscating...\n");
        start = current_time();
        obf = vt->new(mmap, params, args->secparam, args->kappa);
        if (obf == NULL) {
            fprintf(stderr, "error: initializing obfuscator failed\n");
            exit(EXIT_FAILURE);
        }
        if (vt->obfuscate(obf, args->nthreads) == ERR) {
            fprintf(stderr, "error: obfuscation failed\n");
            exit(EXIT_FAILURE);
        }
        end = current_time();
        if (g_verbose)
            fprintf(stderr, "obfuscation: %.2fs\n", end - start);

        start = current_time();
        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        if ((f = fopen(fname, "w")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for writing\n", fname);
            exit(EXIT_FAILURE);
        }
        if (vt->fwrite(obf, f) == ERR) {
            fprintf(stderr, "error: writing obfuscator failed\n");
            exit(EXIT_FAILURE);
        }
        end = current_time();
        if (g_verbose)
            fprintf(stderr, "write to disk: %.2fs\n", end - start);
        vt->free(obf);
        fclose(f);
    }

    if (args->evaluate) {
        char fname[strlen(args->circuit) + 5];
        obfuscation *obf;
        double start, end;
        FILE *f;

        if (g_verbose)
            fprintf(stderr, "evaluating...\n");
        start = current_time();
        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        if ((f = fopen(fname, "r")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for reading\n", fname);
            exit(EXIT_FAILURE);
        }
        if ((obf = vt->fread(mmap, params, f)) == NULL) {
            fprintf(stderr, "error: reading obfuscator failed\n");
            exit(EXIT_FAILURE);
        }
        fclose(f);
        end = current_time();
        if (g_verbose)
            fprintf(stderr, "read from disk: %.2fs\n", end - start);

        if (_evaluate(vt, args, &c, obf) == ERR) {
            fprintf(stderr, "error: evaluation failed\n");
            exit(EXIT_FAILURE);
        }
        vt->free(obf);
    }

cleanup:
    op_vt->free(params);
    acirc_clear(&c);

    return OK;
}

int
main(int argc, char **argv)
{
    int c, idx;
    struct args_t args;

    args_init(&args);

    while ((c = getopt_long(argc, argv, short_opts, opts, &idx)) != -1) {
        switch (c) {
        case 'd':               /* --dry-run */
            args.dry_run = true;
            break;
        case 'D':               /* --debug */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            if (strcmp(optarg, "ERROR") == 0) {
                g_debug = ERROR;
            } else if (strcmp(optarg, "WARN") == 0) {
                g_debug = WARN;
            } else if (strcmp(optarg, "DEBUG") == 0) {
                g_debug = DEBUG;
            } else if (strcmp(optarg, "INFO") == 0) {
                g_debug = INFO;
            } else {
                fprintf(stderr, "error: unknown debug level \"%s\"\n", optarg);
                usage(EXIT_FAILURE);
            }
            break;
        case 'e':               /* --evaluate */
            args.evaluate = true;
            args.obfuscate = false;
            break;
        case 'g':               /* --get-kappa */
            args.get_kappa = true;
            break;
        case 'k':               /* --kappa */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.kappa = atoi(optarg);
            break;
        case 'l':               /* --secparam */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.secparam = atoi(optarg);
            break;
        case 'L':               /* --symlen */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.symlen = atoi(optarg);
            break;
        case 'M':               /* --mmap */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            if (strcmp(optarg, "CLT") == 0) {
                args.mmap = MMAP_CLT;
            } else if (strcmp(optarg, "DUMMY") == 0) {
                args.mmap = MMAP_DUMMY;
            } else {
                fprintf(stderr, "error: unknown mmap \"%s\"\n", optarg);
                usage(EXIT_FAILURE);
            }
            break;
        case 'n': {             /* --npowers */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            const int npowers = atoi(optarg);
            if (npowers <= 0) {
                fprintf(stderr, "error: --npowers argument must be greater than 0\n");
                return EXIT_FAILURE;
            }
            args.npowers = (size_t) npowers;
            break;
        }
        case 'o':               /* --obfuscate */
            args.obfuscate = true;
            args.evaluate = false;
            break;
        case 's':               /* --sigma */
            args.sigma = true;
            break;
        case 'S':               /* --scheme */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            if (strcmp(optarg, "LIN") == 0) {
                args.scheme = SCHEME_LIN;
            } else if (strcmp(optarg, "LZ") == 0) {
                args.scheme = SCHEME_LZ;
            } else {
                fprintf(stderr, "error: unknown scheme \"%s\"\n", optarg);
                usage(EXIT_FAILURE);
            }
            break;
        case 't':               /* --nthreads */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.nthreads = atoi(optarg);
            break;
        case 'v':               /* --verbose */
            g_verbose = true;
            break;
        case 'h':               /* --help */
            usage(EXIT_SUCCESS);
            break;
        default:
            usage(EXIT_FAILURE);
            break;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "error: circuit required\n");
        usage(EXIT_FAILURE);
    } else if (optind == argc - 1) {
        args.circuit = argv[optind];
    } else {
        fprintf(stderr, "error: unexpected argument \"%s\"\n", argv[optind]);
        usage(EXIT_FAILURE);
    }

    if (args.get_kappa) {
        args.mmap = MMAP_DUMMY;
        args.obfuscate = args.evaluate = true;
    } else {
        args_print(&args);
    }

    return run(&args);
}
