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

struct args_t {
    char *circuit;
    enum mmap_e mmap;
    size_t secparam;
    size_t kappa;
    size_t nthreads;
    enum scheme_e scheme;
    bool get_kappa;
    char *evaluate;
    bool obfuscate;
    bool test;
    bool smart;
    /* LIN/LZ specific flags */
    size_t symlen;
    bool sigma;
    /* LZ specific flags */
    size_t npowers;
};

static void
args_init(struct args_t *args)
{
    args->circuit = NULL;
    args->mmap = MMAP_DUMMY;
    args->secparam = 16;
    args->kappa = 0;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->scheme = SCHEME_LZ;
    args->get_kappa = false;
    args->evaluate = NULL;
    args->obfuscate = false;
    args->test = false;
    args->smart = false;
    /* LIN/LZ specific flags */
    args->symlen = 1;
    args->sigma = false;
    /* LZ specific flags */
    args->npowers = 8;
}

static void
args_print(const struct args_t *args)
{
    fprintf(stderr, "Obfuscation details:\n"
            "* Circuit: .......... %s\n"
            "* Multilinear map: .. %s\n"
            "* Security parameter: %lu\n"
            "* Scheme: ........... %s\n"
            "* # threads: ........ %lu\n"
            ,
            args->circuit, mmap_to_string(args->mmap), args->secparam,
            scheme_to_string(args->scheme), args->nthreads);
}

static void
usage(int ret)
{
    struct args_t defaults;
    args_init(&defaults);
    printf("Usage: %s [options] <circuit>\n\n", progname);
    printf(
"  Evaluation flags:\n"
"    --get-kappa           determine the correct κ value for the given circuit\n"
"    --evaluate, -e <INP>  evaluate obfuscation on INP\n"
"    --obfuscate, -o       construct obfuscation\n"
"    --test                obfuscate and evaluate test inputs on circuit (default)\n"
"\n"
"  Execution flags:\n"
"    --debug <LEVEL>   set debug level (options: ERROR, WARN, DEBUG, INFO | default: ERROR)\n"
"    --kappa, -k <κ>   set kappa to κ when obfuscating (default: guess)\n"
"    --lambda, -l <λ>  set security parameter to λ when obfuscating (default: %lu)\n"
"    --nthreads <N>    set the number of threads to N (default: %lu)\n"
"    --scheme <NAME>   set scheme to NAME (options: LIN, LZ | default: %s)\n"
"    --mmap <NAME>     set mmap to NAME (options: CLT, DUMMY | default: %s)\n"
"    --smart           be smart in choosing κ and # powers\n"
"\n"
"  LIN/LZ specific flags:\n"
"    --symlen N  set symbol length to N bits (default: %lu)\n"
"    --sigma     use sigma vectors (default: %s)\n"
"\n"
"  LZ specific flags:\n"
"    --npowers <N>  use N powers (default: %lu)\n"
"\n"
"  Helper flags:\n"
"    --verbose, -v     be verbose\n"
"    --help, -h        print this message\n"
"\n", defaults.secparam, defaults.nthreads,
      scheme_to_string(defaults.scheme), mmap_to_string(defaults.mmap),
      defaults.symlen, defaults.sigma ? "yes" : "no",
      defaults.npowers);
    exit(ret);
}

static const struct option opts[] = {
    /* Evaluation flags */
    {"get-kappa", no_argument, 0, 'g'},
    {"evaluate", required_argument, 0, 'e'},
    {"obfuscate", no_argument, 0, 'o'},
    {"test", no_argument, 0, 'T'},
    /* Execution flags */
    {"debug", required_argument, 0, 'D'},
    {"kappa", required_argument, 0, 'k'},
    {"lambda", required_argument, 0, 'l'},
    {"nthreads", required_argument, 0, 't'},
    {"mmap", required_argument, 0, 'M'},
    {"scheme", required_argument, 0, 'S'},
    {"smart", no_argument, 0, 'r'},
    /* LIN/LZ specific settings */
    {"sigma", no_argument, 0, 's'},
    {"symlen", required_argument, 0, 'L'},
    /* LZ specific settings */
    {"npowers", required_argument, 0, 'n'},
    /* Helper flags */
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};
static const char *short_opts = "aD:egk:lL:M:n:orsS:t:Tvh";

static int
_obfuscate(const obfuscator_vtable *vt, const mmap_vtable *mmap,
           obf_params_t *params, FILE *f, size_t secparam, size_t *kappa,
           size_t nthreads)
{
    obfuscation *obf;
    double start, end, _start, _end;

    if (g_verbose)
        fprintf(stderr, "Obfuscating...\n");

    start = current_time();
    _start = current_time();
    obf = vt->new(mmap, params, NULL, secparam, kappa, nthreads);
    if (obf == NULL) {
        fprintf(stderr, "error: initializing obfuscator failed\n");
        goto error;
    }
    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Initialize: %.2fs\n", _end - _start);

    _start = current_time();
    if (vt->obfuscate(obf, nthreads) == ERR) {
        fprintf(stderr, "error: obfuscation failed\n");
        goto error;
    }
    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Obfuscate: %.2fs\n", _end - _start);
    
    if (f) {
        _start = current_time();
        if (vt->fwrite(obf, f) == ERR) {
            fprintf(stderr, "error: writing obfuscator failed\n");
            goto error;
        }
        _end = current_time();
        if (g_verbose)
            fprintf(stderr, "    Write to disk: %.2fs\n", _end - _start);
    }

    end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Total: %.2fs\n", end - start);

    vt->free(obf);
    return OK;
error:
    vt->free(obf);
    return ERR;
}

static int
_evaluate(const obfuscator_vtable *vt, const mmap_vtable *mmap,
          obf_params_t *params, FILE *f, const int *input, int *output,
          size_t nthreads, size_t *degree, size_t *max_npowers)
{
    double start, end, _start, _end;
    obfuscation *obf;

    if (g_verbose)
        fprintf(stderr, "Evaluating...\n");

    start = current_time();
    _start = current_time();
    if ((obf = vt->fread(mmap, params, f)) == NULL) {
        fprintf(stderr, "error: reading obfuscator failed\n");
        goto error;
    }
    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Read from disk: %.2fs\n", _end - _start);

    _start = current_time();
    if (vt->evaluate(obf, output, input, nthreads, degree, max_npowers) == ERR)
        goto error;
    _end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Evaluate: %.2fs\n", _end - _start);

    end = current_time();
    if (g_verbose)
        fprintf(stderr, "    Total: %.2fs\n", end - start);
    vt->free(obf);
    return OK;
error:
    vt->free(obf);
    return ERR;
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
    size_t kappa = args->kappa;
    size_t npowers = args->npowers;
    int ret = OK;

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
        if (fp == NULL) {
            fprintf(stderr, "error: opening circuit '%s' failed\n", args->circuit);
            exit(EXIT_FAILURE);
        }
        acirc_init(&c);
        if (acirc_fread(&c, fp) == NULL) {
            acirc_clear(&c);
            fclose(fp);
            fprintf(stderr, "error: parsing circuit '%s' failed\n", args->circuit);
            exit(EXIT_FAILURE);
        }
        fclose(fp);
    }

    if (g_verbose) {
        printf("Circuit info:\n");
        printf("* ninputs:  %lu\n", c.ninputs);
        printf("* nconsts:  %lu\n", c.consts.n);
        printf("* noutputs: %lu\n", c.outputs.n);
        printf("* ngates: . %lu\n", c.gates.n);
        printf("* nmuls: .. %lu\n", acirc_nmuls(&c));
        printf("* depth: .. %lu\n", acirc_max_depth(&c));
        printf("* degree: . %lu\n", acirc_max_degree(&c));
        printf("* delta: .. %lu\n", acirc_delta(&c));
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
        lz_params.npowers = npowers;
        lz_params.symlen = args->symlen;
        lz_params.sigma = args->sigma;
        vparams = &lz_params;
        break;
    }

    if (args->smart || args->get_kappa) {
        bool verbosity = g_verbose;
        FILE *f = tmpfile();
        obf_params_t *_params;
        g_verbose = false;

        if (args->smart) {
            printf("Choosing κ%s smartly...\n",
                   args->scheme == SCHEME_LZ ? " and #powers" : "");
        }
        if (f == NULL) {
            fprintf(stderr, "error: unable to open tmpfile\n");
            exit(EXIT_FAILURE);
        }

        _params = op_vt->new(&c, vparams);
        if (_params == NULL) {
            fprintf(stderr, "error: initialize obfuscator parameters failed\n");
            exit(EXIT_FAILURE);
        }
        kappa = 0;
        if (_obfuscate(vt, &dummy_vtable, _params, f, 8, &kappa, args->nthreads) == ERR) {
            fprintf(stderr, "error: unable to obfuscate to determine parameter settings\n");
            exit(EXIT_FAILURE);
        }
        if (args->smart) {
            rewind(f);

            int input[c.ninputs];
            int output[c.outputs.n];

            memset(input, '\0', sizeof input);
            memset(output, '\0', sizeof output);
            if (_evaluate(vt, &dummy_vtable, _params, f, input, output, args->nthreads,
                          &kappa, &npowers) == ERR) {
                fprintf(stderr, "error: unable to evaluate to determine parameter settings\n");
                exit(EXIT_FAILURE);
            }
        }

        fclose(f);
        op_vt->free(_params);
        g_verbose = verbosity;

        if (args->get_kappa) {
            printf("κ = %u\n", kappa);
            acirc_clear(&c);
            return OK;
        }
        if (args->smart) {
            printf("* Setting κ → %u\n", kappa);
            if (args->scheme == SCHEME_LZ) {
                printf("* Setting #powers → %u\n", npowers);
                lz_params.npowers = npowers;

            }
        }
    }

    params = op_vt->new(&c, vparams);
    if (params == NULL) {
        fprintf(stderr, "error: initialize obfuscator parameters failed\n");
        exit(EXIT_FAILURE);
    }

    if (args->obfuscate || args->test) {
        char fname[strlen(args->circuit) + sizeof ".obf\0"];
        FILE *f;

        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        if ((f = fopen(fname, "w")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for writing\n", fname);
            exit(EXIT_FAILURE);
        }
        if (_obfuscate(vt, mmap, params, f, args->secparam, &kappa, args->nthreads) == ERR) {
            exit(EXIT_FAILURE);
        }
        fclose(f);
    }

    if (args->evaluate || args->test) {
        char fname[strlen(args->circuit) + sizeof ".obf\0"];
        FILE *f;

        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        if ((f = fopen(fname, "r")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for reading\n", fname);
            exit(EXIT_FAILURE);
        }

        if (args->evaluate) {
            int input[c.ninputs];
            int output[c.outputs.n];
            for (size_t i = 0; i < c.ninputs; ++i) {
                input[i] = args->evaluate[i] - '0';
            }

            if (_evaluate(vt, mmap, params, f, input, output, args->nthreads,
                          NULL, NULL) == ERR)
                return ERR;
            printf("result: ");
            for (size_t i = 0; i < c.outputs.n; ++i) {
                printf("%d", output[i]);
            }
            printf("\n");
        } else if (args->test) {
            int output[c.outputs.n];
            for (size_t i = 0; i < c.tests.n; i++) {
                bool ok = true;
                rewind(f);
                if (_evaluate(vt, mmap, params, f, c.tests.inps[i], output,
                              args->nthreads, NULL, NULL) == ERR)
                    return ERR;
                for (size_t j = 0; j < c.outputs.n; ++j) {
                    switch (args->scheme) {
                    case SCHEME_LZ:
                        if (!!output[j] != !!c.tests.outs[i][j]) {
                            ok = false;
                            ret = ERR;
                        }
                        break;
                    case SCHEME_LIN:
                        if (output[j] == (c.tests.outs[i][j] != 1)) {
                            ok = false;
                            ret = ERR;
                        }
                        break;
                    }
                }
                if (!ok)
                    printf("\033[1;41m");
                printf("Test #%lu: input=", i);
                array_printstring_rev(c.tests.inps[i], c.ninputs);
                if (ok)
                    printf(" ✓\n");
                else {
                    printf(" ̣✗ expected=");
                    array_printstring_rev(c.tests.outs[i], c.outputs.n);
                    printf(" got=");
                    array_printstring_rev(output, c.outputs.n);
                    printf("\033[0m\n");
                }
            }
        }
        fclose(f);
    }

    op_vt->free(params);
    acirc_clear(&c);

    return ret;
}

int
main(int argc, char **argv)
{
    int c, idx;
    struct args_t args;

    args_init(&args);

    while ((c = getopt_long(argc, argv, short_opts, opts, &idx)) != -1) {
        switch (c) {
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
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.evaluate = optarg;
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
            break;
        case 'r':               /* --smart */
            args.smart = true;
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
        case 'T':
            args.test = true;
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

    if (!args.evaluate && !args.obfuscate && !args.get_kappa)
        args.test = true;

    if (args.get_kappa) {
        args.mmap = MMAP_DUMMY;
    } else {
        args_print(&args);
    }

    return run(&args);
}
