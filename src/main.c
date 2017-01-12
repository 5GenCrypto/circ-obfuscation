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
        return "Lin";
    case SCHEME_LZ:
        return "Linnerman";
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
        return "Dummy";
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
    /* LIN/LZ specific settings */
    size_t symlen;
    bool rachel_inputs;
    /* ZIM/LZ specific settings */
    size_t npowers;

};

static void
args_init(struct args_t *args)
{
    args->circuit = NULL;
    args->mmap = MMAP_CLT;
    args->secparam = 16;
    args->kappa = 0;
    args->nthreads = 1;
    args->evaluate = false;
    args->obfuscate = true;
    args->dry_run = false;
    args->scheme = SCHEME_LZ;
    /* LIN/LZ specific settings */
    args->symlen = 1;
    args->rachel_inputs = false;
    /* ZIM/LZ specific settings */
    args->npowers = 8;
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
"    --all, -a         obfuscate and evaluate\n"
"    --dry-run         don't obfuscate/evaluate\n"
"    --debug <LEVEL>   set debug level (options: ERROR, WARN, DEBUG, INFO | default: ERROR)\n"
"    --evaluate, -e    evaluate obfuscation (default: %s)\n"
"    --obfuscate, -o   construct obfuscation (default: %s)\n"
"    --kappa, -k <κ>   set kappa to κ when obfuscating (default: as chosen by scheme)\n"
"    --lambda, -l <λ>  set security parameter to λ when obfuscating (default: %lu)\n"
"    --nthreads <N>    set the number of threads to N (default: %lu)\n"
"    --scheme <NAME>   set scheme to NAME (options: LIN, LZ | default: LZ)\n"
"    --mmap <NAME>     set mmap to NAME (options: CLT, DUMMY | default: CLT)\n"
"    --verbose, -v     be verbose\n"
"    --help, -h        print this message\n"
"\n"
"  LIN/LZ Specific Settings:\n"
"    --symlen          symbol length (in bits)\n"
"    --rachel          use rachel inputs\n"
"\n"
"  LZ Specific Settings:\n"
"    --npowers <N>     use N powers (default: %lu)\n"
"\n",
           defaults.evaluate ? "yes" : "no",
           defaults.obfuscate ? "yes" : "no",
           defaults.secparam, defaults.nthreads, defaults.npowers);
    exit(ret);
}

static const struct option opts[] = {
    {"all", no_argument, 0, 'a'},
    {"debug", required_argument, 0, 'D'},
    {"dry-run", no_argument, 0, 'd'},
    {"evaluate", no_argument, 0, 'e'},
    {"obfuscate", no_argument, 0, 'o'},
    {"kappa", required_argument, 0, 'k'},
    {"lambda", required_argument, 0, 'l'},
    {"npowers", required_argument, 0, 'n'},
    {"nthreads", required_argument, 0, 't'},
    {"mmap", required_argument, 0, 'M'},
    {"rachel", no_argument, 0, 'r'},
    {"symlen", required_argument, 0, 'L'},
    {"scheme", required_argument, 0, 'S'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};
static const char *short_opts = "adD:ek:lL:M:n:orsS:t:vh";

static int
_evaluate(const obfuscator_vtable *vt, const struct args_t *args,
          const acirc *c, const obfuscation *obf)
{
    int res[c->noutputs];
    int ret = OK;
    double start, end;

    for (size_t i = 0; i < c->ntests; i++) {
        start = current_time();
        if (vt->evaluate(res, c->testinps[i], obf, args->nthreads) == ERR)
            return ERR;
        end = current_time();
        fprintf(stderr, "evaluation: %.2fs\n", end - start);

        bool ok = true;
        for (size_t j = 0; j < c->noutputs; ++j) {
            switch (args->scheme) {
            case SCHEME_LZ:
                if (!!res[j] != !!c->testouts[i][j]) {
                    ok = false;
                    ret = ERR;
                }
                break;
            case SCHEME_LIN:
                if (res[j] == (c->testouts[i][j] != 1)) {
                    ok = false;
                    ret = ERR;
                }
                break;
            }
        }
        if (!ok)
            printf("\033[1;41m");
        printf("test %lu input=", i);
        array_printstring_rev(c->testinps[i], c->ninputs);
        printf(" expected=");
        array_printstring_rev(c->testouts[i], c->noutputs);
        printf(" got=");
        array_printstring_rev(res, c->noutputs);
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

    args_print(args);

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

    acirc_init(&c);
    acirc_verbose(g_verbose);
    if (acirc_parse(&c, args->circuit) == ACIRC_ERR) {
        errx(1, "parsing circuit '%s' failed!", args->circuit);
    }

    if (LOG_DEBUG) {
        fprintf(stderr,
                "circuit: ninputs=%lu nconsts=%lu noutputs=%lu ngates=%lu ntests=%lu nrefs=%lu\n",
                c.ninputs, c.nconsts, c.noutputs, c.ngates, c.ntests, c.nrefs);
    }

    switch (args->scheme) {
    case SCHEME_LIN:
        vt = &lin_obfuscator_vtable;
        op_vt = &lin_op_vtable;
        lin_params.symlen = args->symlen;
        lin_params.rachel_inputs = args->rachel_inputs;
        vparams = &lin_params;
        break;
    case SCHEME_LZ:
        vt = &lz_obfuscator_vtable;
        op_vt = &lz_op_vtable;
        lz_params.npowers = args->npowers;
        lz_params.symlen = args->symlen;
        lz_params.rachel_inputs = args->rachel_inputs;
        vparams = &lz_params;
        break;
    }

    params = op_vt->new(&c, vparams);
    if (params == NULL)
        errx(1, "error: initialize obfuscator parameters failed");

    /* if (g_verbose) */
    /*     acirc_ensure(&c); */

    if (args->dry_run) {
        obfuscation *obf;
        obf = vt->new(mmap, params, args->secparam, args->kappa);
        if (obf == NULL)
            errx(1, "error: initializing obfuscator failed");
        vt->free(obf);
        return OK;
    }

    if (args->obfuscate) {
        char fname[strlen(args->circuit) + 5];
        obfuscation *obf;
        double start, end;
        FILE *f;

        fprintf(stderr, "obfuscating...\n");
        start = current_time();
        obf = vt->new(mmap, params, args->secparam, args->kappa);
        if (obf == NULL)
            errx(1, "error: initializing obfuscator failed");
        if (vt->obfuscate(obf, args->nthreads) == ERR)
            errx(1, "error: obfuscation failed");
        end = current_time();
        fprintf(stderr, "obfuscation: %.2fs\n", end - start);

        start = current_time();
        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        if ((f = fopen(fname, "w")) == NULL)
            errx(1, "error: unable to open '%s' for writing", fname);
        if (vt->fwrite(obf, f) == ERR)
            errx(1, "error: writing obfuscator failed");
        end = current_time();
        fprintf(stderr, "write to disk: %.2fs\n", end - start);
        vt->free(obf);
        fclose(f);
    }

    if (args->evaluate) {
        char fname[strlen(args->circuit) + 5];
        obfuscation *obf;
        double start, end;
        FILE *f;

        fprintf(stderr, "evaluating...\n");
        start = current_time();
        obf = vt->new(mmap, params, args->secparam, args->kappa);
        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        if ((f = fopen(fname, "r")) == NULL)
            errx(1, "error: unable to open '%s' for reading", fname);
        if ((obf = vt->fread(mmap, params, f)) == NULL)
            errx(1, "error: reading obfuscator failed");
        fclose(f);
        end = current_time();
        fprintf(stderr, "read from disk: %.2fs\n", end - start);

        if (_evaluate(vt, args, &c, obf) == ERR)
            errx(1, "error: evaluation failed");
        vt->free(obf);
    }
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
        case 'a':               /* --all */
            args.evaluate = true;
            args.obfuscate = true;
            break;
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
        case 'n':               /* --npowers */
            if (optarg == NULL)
                usage(EXIT_FAILURE);
            args.npowers = atoi(optarg);
            break;
        case 'o':               /* --obfuscate */
            args.obfuscate = true;
            args.evaluate = false;
            break;
        case 'r':               /* --rachel */
            args.rachel_inputs = true;
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

    return run(&args);
}
