#include "dbg.h"
#include "mmap.h"
#include "input_chunker.h"
#include "level.h"
#include "obfuscate.h"
#include "util.h"

#include <aesrand.h>
#include <acirc.h>
#include <mmap/mmap_clt.h>
#include <mmap/mmap_dummy.h>

#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static inline bool ARRAY_EQ(int *xs, int *ys, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (xs[i] != ys[i])
            return false;
    }
    return true;
}

struct args_t {
    char *circuit;
    const mmap_vtable *mmap;
    size_t secparam;
    bool evaluate;
    bool simple;
};

static void
args_init(struct args_t *args)
{
    args->circuit = NULL;
    args->mmap = &clt_vtable;
    args->secparam = 8;
    args->evaluate = false;
    args->simple = false;
}

static void
usage(int ret)
{
    printf("Usage: main [options] <circuit>\n");
    printf("Options:\n"
"    --evaluate, -e   evaluate obfuscation\n"
"    --fake, -f       use dummy multilinear map\n"
"    --simple, -s     ????\n");
    exit(ret);
}

static const struct option opts[] = {
    {"evaluate", no_argument, 0, 'e'},
    {"fake", no_argument, 0, 'f'},
    {"simple", no_argument, 0, 's'},
    {0, 0, 0, 0}
};
static const char *short_opts = "fs";

static obfuscation *
_obfuscate(const struct args_t *const args, const obf_params_t *const params)
{
    obfuscation *obf;

    log_info("obfuscating...");
    obf = obfuscation_new(args->mmap, params, args->secparam);
    obfuscate(obf);
    return obf;
}

static void
_evaluate(const struct args_t *const args, const acirc *const c,
          const obfuscation *const obf)
{
    int res[c->noutputs];

    log_info("evaluating...");
    for (size_t i = 0; i < c->ntests; i++) {
        obfuscation_eval(args->mmap, res, c->testinps[i], obf);
        bool test_ok = ARRAY_EQ(res, c->testouts[i], c->noutputs);
        if (!test_ok)
            printf("\033[1;41m");
        printf("test %lu input=", i);
        array_printstring_rev(c->testinps[i], c->ninputs);
        printf(" expected=");
        array_printstring_rev(c->testouts[i], c->noutputs);
        printf(" got=");
        array_printstring_rev(res, c->noutputs);
        if (!test_ok)
            printf("\033[0m");
        puts("");
    }
}

static int
run(const struct args_t *const args)
{
    obf_params_t params;
    acirc c;
    int ret = 1;

    acirc_init(&c);
    log_info("reading circuit '%s'...", args->circuit);
    if (acirc_parse(&c, args->circuit) == ACIRC_ERR) {
        log_err("parsing circuit failed");
        return 1;
    }

    log_info("circuit: ninputs=%lu nconsts=%lu noutputs=%lu ngates=%lu ntests=%lu nrefs=%lu",
             c.ninputs, c.nconsts, c.noutputs, c.ngates, c.ntests, c.nrefs);
    printf("consts: ");
    array_print(c.consts, c.nconsts);
    puts("");

    obf_params_init(&params, &c, chunker_in_order, rchunker_in_order, args->simple);

    for (size_t i = 0; i < c.noutputs; i++) {
        printf("output bit %lu: type=", i);
        array_print_ui(params.types[i], params.n + params.m + 1);
        puts("");
    }

    acirc_ensure(&c, true);

    if (!args->evaluate) {
        char fname[strlen(args->circuit) + 5];
        obfuscation *obf;
        FILE *f;

        obf = _obfuscate(args, &params);
        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        f = fopen(fname, "w");
        if (f == NULL) {
            log_err("unable to open '%s' for writing", fname);
            goto cleanup;
        }
        obfuscation_fwrite(obf, f);
        obfuscation_free(obf);
        fclose(f);
    } else {
        char fname[strlen(args->circuit) + 5];
        obfuscation *obf;
        FILE *f;

        snprintf(fname, sizeof fname, "%s.obf", args->circuit);
        f = fopen(fname, "r");
        if (f == NULL) {
            log_err("unable to open '%s' for reading", fname);
            goto cleanup;
        }
        obf = obfuscation_fread(args->mmap, &params, f);
        fclose(f);
        if (obf == NULL) {
            log_err("unable to read obfuscation");
            goto cleanup;
        }
        _evaluate(args, &c, obf);
        obfuscation_free(obf);
    }
    ret = 0;
cleanup:
    obf_params_clear(&params);
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
        case 'e':
            args.evaluate = true;
            break;
        case 'f':
            args.mmap = &dummy_vtable;
            break;
        case 's':
            args.simple = true;
            break;
        default:
            usage(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "[%s] error: circuit required\n", __func__);
        usage(EXIT_FAILURE);
    } else if (optind == argc - 1) {
        args.circuit = argv[optind];
    } else {
        fprintf(stderr, "[%s] error: unexpected argument \"%s\"\n", __func__,
                argv[optind]);
        usage(EXIT_FAILURE);
    }

    return run(&args);
}
