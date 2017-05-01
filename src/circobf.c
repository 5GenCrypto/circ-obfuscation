#include "mmap.h"
#include "obfuscator.h"
#include "util.h"

#include "mife/mife.h"
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
    size_t secparam;
    size_t kappa;
    size_t nthreads;
    enum scheme_e scheme;
    bool get_kappa;
    bool obfuscate;
    bool evaluate;
    bool test;
    bool mife_setup;
    bool mife_encrypt;
    bool mife_decrypt;
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
    args->get_kappa = false;
    args->obfuscate = false;
    args->test = false;
    args->mife_setup = args->mife_encrypt = args->mife_decrypt = false;
    args->smart = false;
    args->symlen = 1;
    args->sigma = false;
    args->npowers = 8;
}

static void
args_print(const struct args_t *args)
{
    char *scheme;

    if (args->mife_setup || args->mife_encrypt || args->mife_decrypt)
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
usage(int ret)
{
    struct args_t defaults;
    args_init(&defaults);
    printf("circobf: Circuit-based program obfuscation.\n\n");
    printf("Usage: %s [options] <circuit>\n\n", progname);
    printf(
"  MIFE run flags (only one can be set):\n"
"    --mife-setup            run MIFE setup\n"
"    --mife-encrypt <I> <X>  run MIFE encryption on input X and slot I\n"
"    --mife-decrypt          run MIFE decryption\n"
"\n"        
"  Obfuscation run flags (only one can be set):\n"
"    --get-kappa           print κ value and exit\n"
"    --evaluate, -e <X>    evaluate obfuscation on X\n"
"    --obfuscate, -o       construct obfuscation\n"
"    --test                obfuscate and evaluate on circuit's test inputs\n"
"\n"
"  Obfuscation helper flags:\n"
"    --scheme <NAME>   set scheme to NAME (options: LIN, LZ | default: %s)\n"
"    --sigma           use Σ-vectors (default: %s)\n"
"    --symlen N        set Σ-vector length to N bits (default: %lu)\n"
"    --npowers <N>     use N powers when using LZ (default: %lu)\n"
"\n"
"  Execution flags:\n"
"    --lambda, -l <λ>  set security parameter to λ when obfuscating (default: %lu)\n"
"    --nthreads <N>    set the number of threads to N (default: %lu)\n"
"    --mmap <NAME>     set mmap to NAME (options: CLT, DUMMY | default: %s)\n"
"    --smart           be smart in choosing κ and # powers\n"
"\n"
"  Debugging flags:\n"
"    --debug <LEVEL>   set debug level (options: ERROR, WARN, DEBUG, INFO | default: ERROR)\n"
"    --kappa, -k <Κ>   override default κ choice with Κ\n"
"\n"
"  Helper flags:\n"
"    --verbose, -v     be verbose\n"
"    --help, -h        print this message and exit\n"
"\n", scheme_to_string(defaults.scheme), 
      defaults.sigma ? "yes" : "no",
      defaults.symlen,
      defaults.npowers,
      defaults.secparam, defaults.nthreads,
      mmap_to_string(defaults.mmap));
    exit(ret);
}

static const struct option opts[] = {
    /* MIFE run flags */
    {"mife-setup", no_argument, 0, 'A'},
    {"mife-encrypt", required_argument, 0, 'B'},
    
    /* Obfuscation run flags */
    {"get-kappa", no_argument, 0, 'g'},
    {"evaluate", required_argument, 0, 'e'},
    {"obfuscate", no_argument, 0, 'o'},
    {"test", no_argument, 0, 'T'},
    /* Obfuscation helper flags */
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
    {"debug", required_argument, 0, 'D'},
    {"kappa", required_argument, 0, 'k'},
    /* Helper flags */
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};
static const char *short_opts = "AD:e:ghk:lL:M:n:orsS:t:Tv";

static int
mife_setup_run(const struct args_t *args, const mmap_vtable *mmap,
               circ_params_t *cp, aes_randstate_t rng)
{
    char skname[strlen(args->circuit) + sizeof ".sk\0"];
    char ekname[strlen(args->circuit) + sizeof ".ek\0"];
    mife_t *mife;
    mife_sk_t *sk;
    mife_ek_t *ek;
    FILE *fp;

    mife = mife_setup(mmap, cp, args->secparam, rng, NULL, args->nthreads);
    if (mife == NULL)
        return ERR;
    sk = mife_sk(mife);
    snprintf(skname, sizeof skname, "%s.sk", args->circuit);
    if ((fp = fopen(skname, "w")) == NULL) {
        fprintf(stderr, "error: unable to open '%s' for writing\n", skname);
        exit(EXIT_FAILURE);
    }
    mife_sk_fwrite(sk, fp);
    fclose(fp);
    mife_sk_free(sk);

    ek = mife_ek(mife);
    snprintf(ekname, sizeof ekname, "%s.ek", args->circuit);
    if ((fp = fopen(ekname, "w")) == NULL) {
        fprintf(stderr, "error: unable to open '%s' for writing\n", ekname);
        exit(EXIT_FAILURE);
    }
    mife_ek_fwrite(ek, fp);
    fclose(fp);
    mife_ek_free(ek);

    mife_free(mife);

    return OK;
}

static int
mife_encrypt_run(const struct args_t *args, const mmap_vtable *mmap,
                 circ_params_t *cp, aes_randstate_t rng)
{
    char skname[strlen(args->circuit) + sizeof ".sk\0"];
    mife_ciphertext_t *ct;
    mife_sk_t *sk;
    size_t ninputs;
    FILE *fp;

    if (args->mife_slot >= cp->n) {
        fprintf(stderr, "error: invalid MIFE slot\n");
        exit(EXIT_FAILURE);
    }
    ninputs = cp->ds[args->mife_slot];
    if (strlen(args->input) != ninputs) {
        fprintf(stderr, "error: invalid number of inputs\n");
        exit(EXIT_FAILURE);
    }

    size_t input[ninputs];

    if (g_verbose) {
        fprintf(stderr,
                "MIFE encryption details:\n"
                "* Slot: .... %lu\n"
                "* Input: ... %s\n",
                args->mife_slot, args->input);
    }

    snprintf(skname, sizeof skname, "%s.sk", args->circuit);
    if ((fp = fopen(skname, "r")) == NULL) {
        fprintf(stderr, "error: unable to open '%s' for reading\n", skname);
        exit(EXIT_FAILURE);
    }
    sk = mife_sk_fread(mmap, cp, fp);
    fclose(fp);

    for (size_t i = 0; i < ninputs; ++i) {
        input[i] = args->input[i] - '0';
    }

    ct = mife_encrypt(sk, args->mife_slot, input, args->nthreads, rng);
    if (ct == NULL) {
        fprintf(stderr, "MIFE encryption failed\n");
        exit(EXIT_FAILURE);
    }

    {
        char ctname[strlen(args->circuit) + 10 + strlen("..ct\0")];
        char slot[10];

        snprintf(slot, sizeof slot, "%lu", args->mife_slot);
        snprintf(ctname, sizeof ctname, "%s.%s.ct", args->circuit, slot);

        if ((fp = fopen(ctname, "w")) == NULL) {
            fprintf(stderr, "error: unable to open '%s' for writing\n", ctname);
            exit(EXIT_FAILURE);
        }
        mife_ciphertext_fwrite(ct, cp, fp);
    }
    mife_ciphertext_free(ct, cp);
    mife_sk_free(sk);

    return OK;
}

static int
mife_run(const struct args_t *args, const mmap_vtable *mmap, acirc *circ)
{
    circ_params_t cp;
    aes_randstate_t rng;
    int ret = ERR;

    aes_randinit(rng);
    circ_params_init(&cp, circ->ninputs, circ);
    for (size_t i = 0; i < cp.n; ++i) {
        cp.ds[i] = 1;
        cp.qs[i] = 1 << 1;
    }
    if (g_verbose) {
        fprintf(stderr, "Circuit parameters:\n");
        fprintf(stderr, "* n: ......... %lu\n", cp.n);
        for (size_t i = 0; i < cp.n; ++i) {
            fprintf(stderr, "*   %lu: ....... %lu (%lu)\n", i + 1, cp.ds[i], cp.qs[i]);
        }
        fprintf(stderr, "* m: ......... %lu\n", cp.m);
        if (args->mife_setup || args->mife_encrypt)
            fprintf(stderr, "* # encodings: %lu\n",
                    args->mife_setup ? mife_params_num_encodings_setup(&cp)
                                     : mife_params_num_encodings_encrypt(&cp, args->mife_slot));
    }
    
    if (args->mife_setup) {
        ret = mife_setup_run(args, mmap, &cp, rng);
    } else if (args->mife_encrypt) {
        ret = mife_encrypt_run(args, mmap, &cp, rng);
    } else if (args->mife_decrypt) {
        
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

    if (args->smart || args->get_kappa) {
        bool verbosity = g_verbose;
        FILE *f = tmpfile();
        obf_params_t *_params;
        /* XXX: TEMPORARY */
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

        if (args->get_kappa) {
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
        } else if (args->test) {
            int output[circ->outputs.n];
            for (size_t i = 0; i < circ->tests.n; i++) {
                bool ok = true;
                rewind(f);
                if (_evaluate(vt, mmap, params, f, circ->tests.inps[i], output,
                              args->nthreads, NULL, NULL) == ERR)
                    return ERR;
                for (size_t j = 0; j < circ->outputs.n; ++j) {
                    switch (args->scheme) {
                    case SCHEME_LZ:
                        if (!!output[j] != !!circ->tests.outs[i][j]) {
                            ok = false;
                            ret = ERR;
                        }
                        break;
                    case SCHEME_LIN:
                        if (output[j] == (circ->tests.outs[i][j] != 1)) {
                            ok = false;
                            ret = ERR;
                        }
                        break;
                    }
                }
                if (!ok)
                    printf("\033[1;41m");
                printf("Test #%lu: input=", i);
                array_printstring_rev(circ->tests.inps[i], circ->ninputs);
                if (ok)
                    printf(" ✓\n");
                else {
                    printf(" ̣✗ expected=");
                    array_printstring_rev(circ->tests.outs[i], circ->outputs.n);
                    printf(" got=");
                    array_printstring_rev(output, circ->outputs.n);
                    printf("\033[0m\n");
                }
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
    int ret;

    args_init(&args);

    while ((c = getopt_long(argc, argv, short_opts, opts, &idx)) != -1) {
        switch (c) {
        case 'A':               /* --mife-setup */
            args.mife_setup = true;
            break;
        case 'B':               /* --mife-encrypt */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.mife_encrypt = true;
            {
                char *tok;
                tok = strsep(&optarg, " ");
                args.mife_slot = atoi(tok);
                tok = strsep(&optarg, " ");
                args.input = tok;
            }
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
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.evaluate = true;
            args.input = optarg;
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

    int mife_flags = (int) args.mife_setup
        + (int) args.mife_encrypt
        + (int) args.mife_decrypt;
    int obf_flags = (int) args.evaluate
        + (int) args.obfuscate
        + (int) args.get_kappa
        + (int) args.test;

    if (mife_flags && obf_flags) {
        fprintf(stderr, "error: cannot use both MIFE flags and obfuscation flags\n");
        usage(EXIT_FAILURE);
    }
    if (!mife_flags && !obf_flags) {
        fprintf(stderr, "error: must use either MIFE flags or obfuscation flags\n");
        usage(EXIT_FAILURE);
    }
    if (mife_flags > 1) {
        fprintf(stderr, "error: only one of --mife-setup, --mife-encrypt, --mife-evaluate can be used\n");
        usage(EXIT_FAILURE);
    }
    if (obf_flags > 1) {
        fprintf(stderr, "error: only one of --get-kappa, --obfuscate, --evaluate, --test can be used\n");
        usage(EXIT_FAILURE);
    }

    if (args.get_kappa)
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
        FILE *fp = fopen(args.circuit, "r");
        if (fp == NULL) {
            fprintf(stderr, "error: opening circuit '%s' failed\n", args.circuit);
            exit(EXIT_FAILURE);
        }
        acirc_init(&circ);
        if (acirc_fread(&circ, fp) == NULL) {
            acirc_clear(&circ);
            fclose(fp);
            fprintf(stderr, "error: parsing circuit '%s' failed\n", args.circuit);
            exit(EXIT_FAILURE);
        }
        fclose(fp);
    }

    if (g_verbose) {
        printf("Circuit info:\n");
        printf("* ninputs:  %lu\n", circ.ninputs);
        printf("* nconsts:  %lu\n", circ.consts.n);
        printf("* noutputs: %lu\n", circ.outputs.n);
        printf("* ngates: . %lu\n", circ.gates.n);
        printf("* nmuls: .. %lu\n", acirc_nmuls(&circ));
        printf("* depth: .. %lu\n", acirc_max_depth(&circ));
        printf("* degree: . %lu\n", acirc_max_degree(&circ));
        printf("* delta: .. %lu\n", acirc_delta(&circ));
    }
    
    if (mife_flags)
        ret = mife_run(&args, mmap, &circ);
    if (obf_flags)
        ret = obf_run(&args, mmap, &circ);

    acirc_clear(&circ);

    return ret;
}
