#include "dbg.h"
#include "mmap.h"
#include "evaluate.h"
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
    args->secparam = 16;
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

static int
run(struct args_t *args)
{
    obf_params op;
    acirc c;

    acirc_init(&c);
    log_info("reading circuit '%s'...", args->circuit);
    acirc_parse(&c, args->circuit);

    printf("circuit: ninputs=%lu nconsts=%lu noutputs=%lu ngates=%lu ntests=%lu nrefs=%lu\n",
           c.ninputs, c.nconsts, c.noutputs, c.ngates, c.ntests, c.nrefs);

    obf_params_init(&op, &c, chunker_in_order, rchunker_in_order, args->simple);
    printf("consts: ");
    array_print(c.consts, c.nconsts);
    puts("");

    for (size_t i = 0; i < c.noutputs; i++) {
        printf("output bit %lu: type=", i);
        array_print_ui(op.types[i], op.n + op.m + 1);
        puts("");
    }

    acirc_ensure(&c, true);

    aes_randstate_t rng;
    aes_randinit(rng);

    printf("initializing params..\n");
    secret_params st;
    secret_params_init(args->mmap, &st, &op, args->secparam, rng);
    public_params pp;
    public_params_init(args->mmap, &pp, &st);

    printf("obfuscating...\n");
    obfuscation *obf;
    obf = obfuscation_new(args->mmap, &pp);
    obfuscate(obf, &st, rng);

    puts("evaluating...");
    int res[c.noutputs];
    for (size_t i = 0; i < c.ntests; i++) {
        evaluate(args->mmap, res, c.testinps[i], obf, &pp);
        bool test_ok = ARRAY_EQ(res, c.testouts[i], c.noutputs);
        if (!test_ok)
            printf("\033[1;41m");
        printf("test %lu input=", i);
        array_printstring_rev(c.testinps[i], c.ninputs);
        printf(" expected=");
        array_printstring_rev(c.testouts[i], c.noutputs);
        printf(" got=");
        array_printstring_rev(res, c.noutputs);
        if (!test_ok)
            printf("\033[0m");
        puts("");
    }

    // free all the things
    aes_randclear(rng);
    obfuscation_free(obf);
    public_params_clear(args->mmap, &pp);
    secret_params_clear(args->mmap, &st);
    obf_params_clear(&op);
    acirc_clear(&c);

    return 0;
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
                argv[optind + 1]);
        usage(EXIT_FAILURE);
    }

    run(&args);

}
