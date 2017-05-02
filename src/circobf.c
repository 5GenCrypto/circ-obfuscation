#include "mmap.h"
#include "obfuscator.h"
#include "util.h"

#include "mife/mife_run.h"
#include "mife/mife_params.h"
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

enum mife_e {
    MIFE_NONE = 0,
    MIFE_SETUP,
    MIFE_ENCRYPT,
    MIFE_DECRYPT,
    MIFE_TEST,
    MIFE_KAPPA,
};

enum obf_e {
    OBF_NONE = 0,
    OBF_OBFUSCATE,
    OBF_EVALUATE,
    OBF_TEST,
    OBF_KAPPA,
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
    char *input;
    enum mmap_e mmap;
    enum mife_e mife;
    enum obf_e obf;
    size_t secparam;
    size_t kappa;
    size_t nthreads;
    enum scheme_e scheme;
    size_t mife_slot;
    bool smart;
    size_t symlen;
    bool sigma;
    size_t npowers;
};

static void
args_init(struct args_t *args)
{
    args->circuit = NULL;
    args->input = NULL;
    args->mmap = MMAP_DUMMY;
    args->secparam = 16;
    args->kappa = 0;
    args->nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    args->scheme = SCHEME_LZ;
    args->obf = OBF_NONE;
    args->mife = MIFE_NONE;
    args->smart = false;
    args->symlen = 1;
    args->sigma = false;
    args->npowers = 8;
}

static void
args_print(const struct args_t *args)
{
    char *scheme;

    if (args->mife != MIFE_NONE)
        scheme = "MIFE";
    else
        scheme = scheme_to_string(args->scheme);
    fprintf(stderr, "Info:\n"
            "* Circuit: .......... %s\n"
            "* Multilinear map: .. %s\n"
            "* Security parameter: %lu\n"
            "* Scheme: ........... %s\n"
            "* # threads: ........ %lu\n"
            ,
            args->circuit, mmap_to_string(args->mmap), args->secparam,
            scheme, args->nthreads);
}

static void
usage(bool longform, int ret)
{
    struct args_t defaults;
    args_init(&defaults);
    if (longform)
        printf("circobf: Circuit-based program obfuscation.\n\n");
    printf("usage: %s [options] <circuit>\n", progname);
    if (longform)
        printf("\n"
"  MIFE:\n"
"    --mife-setup              run setup procedure\n"
"    --mife-encrypt <I> <X>    run encryption on input X and slot I\n"
"    --mife-decrypt <E> <C>... run decryption on evaluation key E and ciphertexts C...\n"
"    --mife-test               run MIFE on circuit's test inputs\n"
"    --mife-kappa              print κ value and exit\n"
"\n"        
"  Obfuscation:\n"
"    --obf-evaluate <X>    run evaluation on input X\n"
"    --obf-obfuscate       run obfuscation procedure\n"
"    --obf-test            run obfuscation on circuit's test inputs\n"
"    --obf-kappa           print κ value and exit\n"
"    --scheme <NAME>       set scheme to NAME (options: LIN, LZ | default: %s)\n"
"    --sigma               use Σ-vectors (default: %s)\n"
"    --symlen N            set Σ-vector length to N bits (default: %lu)\n"
"\n"
"  Other:\n"
"    --lambda, -l <λ>  set security parameter to λ when obfuscating (default: %lu)\n"
"    --nthreads <N>    set the number of threads to N (default: %lu)\n"
"    --mmap <NAME>     set mmap to NAME (options: CLT, DUMMY | default: %s)\n"
"    --smart           be smart in choosing κ and # powers\n"
"    --npowers <N>     use N powers when using LZ (default: %lu)\n"
"\n"
"  Helper flags:\n"
"    --debug <LEVEL>   set debug level (options: ERROR, WARN, DEBUG, INFO | default: ERROR)\n"
"    --kappa, -k <Κ>   override default κ choice with Κ\n"
"    --verbose, -v     be verbose\n"
"    --help, -h        print this message and exit\n"
"\n", scheme_to_string(defaults.scheme), 
      defaults.sigma ? "yes" : "no",
      defaults.symlen,
      defaults.secparam,
      defaults.nthreads,
      mmap_to_string(defaults.mmap),
      defaults.npowers
      );
    exit(ret);
}

static const struct option opts[] = {
    {"mife-setup", no_argument, 0, 'A'},
    {"mife-encrypt", required_argument, 0, 'B'},
    {"mife-decrypt", required_argument, 0, 'C'},
    {"mife-test", no_argument, 0, 'D'},
    {"mife-kappa", no_argument, 0, 'E'},

    {"obf-evaluate", required_argument, 0, 'e'},
    {"obf-obfuscate", no_argument, 0, 'o'},
    {"obf-test", no_argument, 0, 'T'},
    {"obf-kappa", no_argument, 0, 'g'},

    {"scheme", required_argument, 0, 'S'},
    {"sigma", no_argument, 0, 's'},
    {"symlen", required_argument, 0, 'L'},
    {"npowers", required_argument, 0, 'n'},
    /* Execution flags */
    {"lambda", required_argument, 0, 'l'},
    {"nthreads", required_argument, 0, 't'},
    {"mmap", required_argument, 0, 'M'},
    {"smart", no_argument, 0, 'r'},
    /* Debugging flags */
    {"debug", required_argument, 0, 'd'},
    {"kappa", required_argument, 0, 'k'},
    /* Helper flags */
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};
static const char *short_opts = "AB:C:DEd:e:ghk:lL:M:n:orsS:t:Tv";

static int
mife_run(const struct args_t *args, const mmap_vtable *mmap, acirc *circ)
{
    circ_params_t cp;
    aes_randstate_t rng;
    int ret = ERR;
    size_t consts = 0;
    size_t kappa = args->kappa;
    size_t npowers = args->npowers;
    size_t symlen = args->sigma ? args->symlen : 1;

    if (circ->ninputs % symlen != 0) {
        fprintf(stderr, "error: Σ-vector length must be divisible by number of inputs\n");
        return ERR;
    }
    
    aes_randinit(rng);
    if (circ->consts.n > 0)
        consts = 1;
    circ_params_init(&cp, circ->ninputs / symlen + consts, circ);
    for (size_t i = 0; i < cp.n; ++i) {
        cp.ds[i] = symlen;
        cp.qs[i] = args->sigma ? symlen : (size_t) 1 << symlen;
    }
    if (consts) {
        cp.ds[cp.n - 1] = circ->consts.n;
        cp.qs[cp.n - 1] = 1;
    }
    if (g_verbose) {
        fprintf(stderr, "Circuit parameters:\n");
        fprintf(stderr, "* n: ......... %lu\n", cp.n);
        for (size_t i = 0; i < cp.n; ++i) {
            fprintf(stderr, "*   %lu: ....... %lu (%lu)\n", i + 1, cp.ds[i], cp.qs[i]);
        }
        fprintf(stderr, "* m: ......... %lu\n", cp.m);
        if (args->mife == MIFE_SETUP || args->mife == MIFE_ENCRYPT)
            fprintf(stderr, "* # encodings: %lu\n",
                    args->mife == MIFE_SETUP ? mife_params_num_encodings_setup(&cp)
                                             : mife_params_num_encodings_encrypt(&cp, args->mife_slot, npowers));
    }

    if (args->smart || args->mife == MIFE_KAPPA) {
        bool verbosity = g_verbose;
        /* g_verbose = false; */

        if (args->smart) {
            printf("Choosing κ smartly...\n");
        }

        ret = mife_run_setup(mmap, args->circuit, &cp, rng, args->secparam, &kappa, args->nthreads);
        if (ret == ERR) {
            fprintf(stderr, "error: mife setup failed\n");
            return ERR;
        }
        if (args->smart) {
            int *inps[cp.n - consts];
            for (size_t i = 0; i < cp.n - consts; ++i) {
                inps[i] = my_calloc(cp.ds[i], sizeof inps[i][0]);
            }
            if (mife_run_all(mmap, args->circuit, &cp, rng, inps, NULL,
                             &kappa, &npowers, args->nthreads) == ERR) {
                fprintf(stderr, "error: unable to determine parameter settings\n");
                return ERR;
            }
            for (size_t i = 0; i < cp.n - consts; ++i) {
                free(inps[i]);
            }
        }

        g_verbose = verbosity;
        if (args->mife == MIFE_KAPPA) {
            printf("κ = %lu\n", kappa);
            return OK;
        }
        if (args->smart) {
            printf("* Setting κ → %lu\n", kappa);
            /* XXX: printf("* Setting #powers → %lu\n", npowers); */
        }
    }

    switch (args->mife) {
    case MIFE_SETUP:
        ret = mife_run_setup(mmap, args->circuit, &cp, rng, args->secparam,
                             &kappa, args->nthreads);
        break;
    case MIFE_ENCRYPT: {
        /* XXX: FIXME */
        /* int input[strlen(args->input)]; */
        /* for (size_t i = 0; i < strlen(args->input); ++i) { */
        /*     input[i] = args->input[i] - '0'; */
        /* } */
        /* ret = mife_run_encrypt(mmap, args->circuit, &cp, rng, input, */
        /*                        args->mife_slot, &npowers, args->nthreads, NULL); */
        break;
    }
    case MIFE_DECRYPT: {
        char *tofree, *str, *ek, *cts[cp.n];
        int rop[cp.m];
        tofree = str = strdup(args->input);
        ek = strsep(&str, " ");
        if (ek == NULL) {
            fprintf(stderr, "error: missing evaluation key\n");
            goto cleanup;
        }
        for (size_t i = 0; i < cp.n; ++i) {
            if ((cts[i] = strsep(&str, " ")) == NULL) {
                fprintf(stderr, "error: too few ciphertexts given (got %lu, need %lu)\n",
                        i + 1, cp.n);
                goto cleanup;
            }
        }
        if (strsep(&str, " ") != NULL) {
            fprintf(stderr, "error: too many ciphertexts given (need %lu)\n",
                    cp.n);
            goto cleanup;
        }
        ret = mife_run_decrypt(ek, cts, rop, mmap, &cp, &kappa, args->nthreads);
        fprintf(stderr, "result: ");
        for (size_t o = 0; o < cp.m; ++o) {
            fprintf(stderr, "%d", rop[o]);
        }
        fprintf(stderr, "\n");
cleanup:
        free(tofree);
        break;
    }
    case MIFE_TEST:
        ret = mife_run_test(mmap, args->circuit, &cp, rng, args->secparam,
                            &kappa, &npowers, args->nthreads);
        break;
    default:
        abort();
    }
    aes_randclear(rng);
    return ret;
}

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
          size_t nthreads, size_t *kappa, size_t *max_npowers)
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
    if (vt->evaluate(obf, output, input, nthreads, kappa, max_npowers) == ERR)
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
obf_run(const struct args_t *args, const mmap_vtable *mmap, acirc *circ)
{
    obf_params_t *params;
    const obfuscator_vtable *vt;
    const op_vtable *op_vt;
    lin_obf_params_t lin_params;
    lz_obf_params_t lz_params;
    void *vparams;
    size_t kappa = args->kappa;
    size_t npowers = args->npowers;
    int ret = OK;

    switch (args->scheme) {
    case SCHEME_LIN:
        fprintf(stderr, "LIN SCHEME BROKEN!\n");
        exit(1);
        /* vt = &lin_obfuscator_vtable; */
        /* op_vt = &lin_op_vtable; */
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

    if (args->smart || args->obf == OBF_KAPPA) {
        bool verbosity = g_verbose;
        FILE *f = tmpfile();
        obf_params_t *_params;
        /* XXX: */
        /* g_verbose = false; */

        if (args->smart) {
            printf("Choosing κ%s smartly...\n",
                   args->scheme == SCHEME_LZ ? " and #powers" : "");
        }
        if (f == NULL) {
            fprintf(stderr, "error: unable to open tmpfile\n");
            exit(EXIT_FAILURE);
        }

        _params = op_vt->new(circ, vparams);
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

            int input[circ->ninputs];
            int output[circ->outputs.n];

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

        if (args->obf == OBF_KAPPA) {
            printf("κ = %lu\n", kappa);
            return OK;
        }
        if (args->smart) {
            printf("* Setting κ → %lu\n", kappa);
            if (args->scheme == SCHEME_LZ) {
                printf("* Setting #powers → %lu\n", npowers);
                lz_params.npowers = npowers;
            }
        }
    }

    params = op_vt->new(circ, vparams);
    if (params == NULL) {
        fprintf(stderr, "error: initialize obfuscator parameters failed\n");
        exit(EXIT_FAILURE);
    }

    if (args->obf == OBF_OBFUSCATE || args->obf == OBF_TEST) {
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

    if (args->obf == OBF_EVALUATE || args->obf == OBF_TEST) {
        char fname[strlen(args->circuit) + sizeof ".obf\0"];
        FILE *f;

        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        if ((f = fopen(fname, "r")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for reading\n", fname);
            exit(EXIT_FAILURE);
        }

        if (args->obf == OBF_EVALUATE) {
            int input[circ->ninputs];
            int output[circ->outputs.n];
            assert(args->input);
            for (size_t i = 0; i < circ->ninputs; ++i) {
                input[i] = args->input[i] - '0';
            }

            if (_evaluate(vt, mmap, params, f, input, output, args->nthreads,
                          NULL, NULL) == ERR)
                return ERR;
            printf("result: ");
            for (size_t i = 0; i < circ->outputs.n; ++i) {
                printf("%d", output[i]);
            }
            printf("\n");
        } else if (args->obf == OBF_TEST) {
            int output[circ->outputs.n];
            for (size_t i = 0; i < circ->tests.n; i++) {
                rewind(f);
                if (_evaluate(vt, mmap, params, f, circ->tests.inps[i], output,
                              args->nthreads, NULL, NULL) == ERR)
                    return ERR;
                if (args->scheme == SCHEME_LIN) {
                    for (size_t j = 0; j < circ->outputs.n; ++j)
                        output[j] = !output[j];
                    /*             if (output[j] == (circ->tests.outs[i][j] != 1)) { */
                }
                if (!print_test_output(i + 1, circ->tests.inps[i], circ->ninputs,
                                       circ->tests.outs[i], output,
                                       circ->outputs.n))
                    ret = ERR;
            }
        }
        fclose(f);
    }

    op_vt->free(params);

    return ret;
}

int
main(int argc, char **argv)
{
    int c, idx;
    struct args_t args;
    const mmap_vtable *mmap;
    acirc circ;
    int ret = ERR;

    args_init(&args);

    while ((c = getopt_long(argc, argv, short_opts, opts, &idx)) != -1) {
        switch (c) {
        case 'A':               /* --mife-setup */
            if (args.mife != MIFE_NONE)
                usage(true, EXIT_FAILURE);
            args.mife = MIFE_SETUP;
            break;
        case 'B':               /* --mife-encrypt */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            if (args.mife != MIFE_NONE)
                usage(true, EXIT_FAILURE);
            args.mife = MIFE_ENCRYPT;
            {
                char *tok;
                tok = strsep(&optarg, " ");
                args.mife_slot = atoi(tok);
                tok = strsep(&optarg, " ");
                args.input = tok;
            }
            break;
        case 'C':               /* --mife-decrypt */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            if (args.mife != MIFE_NONE)
                usage(true, EXIT_FAILURE);
            args.mife = MIFE_DECRYPT;
            args.input = optarg;
            break;
        case 'D':               /* --mife-test */
            if (args.mife != MIFE_NONE)
                usage(true, EXIT_FAILURE);
            args.mife = MIFE_TEST;
            break;
        case 'E':               /* --mife-kappa */
            if (args.mife != MIFE_NONE)
                usage(true, EXIT_FAILURE);
            args.mife = MIFE_KAPPA;
            break;
        case 'd':               /* --debug */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
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
                usage(true, EXIT_FAILURE);
            }
            break;
        case 'e':               /* --obf-evaluate */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            if (args.obf != OBF_NONE)
                usage(true, EXIT_FAILURE);
            args.obf = OBF_EVALUATE;
            args.input = optarg;
            break;
        case 'g':               /* --obf-kappa */
            if (args.obf != OBF_NONE)
                usage(true, EXIT_FAILURE);
            args.obf = OBF_KAPPA;
            break;
        case 'k':               /* --kappa */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            args.kappa = atoi(optarg);
            break;
        case 'l':               /* --secparam */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            args.secparam = atoi(optarg);
            break;
        case 'L':               /* --symlen */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            args.symlen = atoi(optarg);
            break;
        case 'M':               /* --mmap */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            if (strcmp(optarg, "CLT") == 0) {
                args.mmap = MMAP_CLT;
            } else if (strcmp(optarg, "DUMMY") == 0) {
                args.mmap = MMAP_DUMMY;
            } else {
                fprintf(stderr, "error: unknown mmap \"%s\"\n", optarg);
                usage(true, EXIT_FAILURE);
            }
            break;
        case 'n': {             /* --npowers */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            const int npowers = atoi(optarg);
            if (npowers <= 0) {
                fprintf(stderr, "error: --npowers argument must be greater than 0\n");
                return EXIT_FAILURE;
            }
            args.npowers = (size_t) npowers;
            break;
        }
        case 'o':               /* --obf-obfuscate */
            if (args.obf != OBF_NONE)
                usage(true, EXIT_FAILURE);
            args.obf = OBF_OBFUSCATE;
            break;
        case 'r':               /* --smart */
            args.smart = true;
            break;
        case 's':               /* --sigma */
            args.sigma = true;
            break;
        case 'S':               /* --scheme */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            if (strcmp(optarg, "LIN") == 0) {
                args.scheme = SCHEME_LIN;
            } else if (strcmp(optarg, "LZ") == 0) {
                args.scheme = SCHEME_LZ;
            } else {
                fprintf(stderr, "error: unknown scheme \"%s\"\n", optarg);
                usage(true, EXIT_FAILURE);
            }
            break;
        case 't':               /* --nthreads */
            if (optarg == NULL)
                usage(true, EXIT_FAILURE);
            args.nthreads = atoi(optarg);
            break;
        case 'T':
            if (args.obf != OBF_NONE)
                usage(true, EXIT_FAILURE);
            args.obf = OBF_TEST;
            break;
        case 'v':               /* --verbose */
            g_verbose = true;
            break;
        case 'h':               /* --help */
            usage(true, EXIT_SUCCESS);
            break;
        default:
            usage(false, EXIT_FAILURE);
            break;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "error: circuit required\n");
        usage(false, EXIT_FAILURE);
    } else if (optind == argc - 1) {
        args.circuit = argv[optind];
    } else {
        fprintf(stderr, "error: unexpected argument \"%s\"\n", argv[optind]);
        usage(false, EXIT_FAILURE);
    }

    if (args.mife != MIFE_NONE && args.obf != OBF_NONE) {
        fprintf(stderr, "error: cannot use both MIFE flags and obfuscation flags\n");
        usage(false, EXIT_FAILURE);
    }
    if (args.mife == MIFE_NONE && args.obf == OBF_NONE) {
        fprintf(stderr, "error: one of MIFE flags or obfuscation flags must be used\n");
        usage(false, EXIT_FAILURE);
    }

    if (args.mife == MIFE_KAPPA || args.obf == OBF_KAPPA)
        args.mmap = MMAP_DUMMY;
    if (g_verbose)
        args_print(&args);

    switch (args.mmap) {
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
        FILE *fp;
        void *res;

        if ((fp = fopen(args.circuit, "r")) == NULL) {
            fprintf(stderr, "error: opening circuit '%s' failed\n", args.circuit);
            exit(EXIT_FAILURE);
        }
        acirc_init(&circ);
        res = acirc_fread(&circ, fp);
        fclose(fp);
        if (res == NULL) {
            fprintf(stderr, "error: parsing circuit '%s' failed\n", args.circuit);
            goto cleanup;
        }
    }

    if (g_verbose) {
        printf("Circuit info:\n");
        printf("* ninputs: .. %lu\n", circ.ninputs);
        printf("* nconsts: .. %lu\n", circ.consts.n);
        printf("* noutputs: . %lu\n", circ.outputs.n);
        printf("* ngates: ... %lu\n", circ.gates.n);
        printf("* nmuls: .... %lu\n", acirc_nmuls(&circ));
        printf("* depth: .... %lu\n", acirc_max_depth(&circ));
        printf("* degree: ... %lu\n", acirc_max_degree(&circ));
        printf("* delta: .... %lu\n", acirc_delta(&circ));
    }
    
    if (args.mife != MIFE_NONE)
        ret = mife_run(&args, mmap, &circ);
    if (args.obf != OBF_NONE)
        ret = obf_run(&args, mmap, &circ);

cleanup:
    acirc_clear(&circ);
    return ret;
}
