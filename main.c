#include "aesrand.h"
#include "mmap.h"
#include "evaluate.h"
#include "input_chunker.h"
#include "level.h"
#include "obfuscate.h"
#include "util.h"

#include <acirc.h>
#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <mmap/mmap_clt.h>
#include <mmap/mmap_dummy.h>

// obfuscate
// save circuit w/o consts
// evaluate

static void
usage(int ret)
{
    printf("Usage: main [options] [circuit]\n");
    printf("Options:\n");
    printf("\t-f\tUse fake multilinear map\n");
    exit(ret);
}

int
main(int argc, char **argv)
{
    size_t lambda = 16;
    char *fname;
    acirc c;
    obf_params op;
    int arg;
    bool simple = false;

    const mmap_vtable *mmap = &clt_vtable;

    while ((arg = getopt(argc, argv, "fs:")) != -1) {
        switch (arg) {
        case 'f':
            mmap = &dummy_vtable;
            break;
        default:
            usage(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "[%s] error: circuit required\n", __func__);
        usage(EXIT_FAILURE);
    } else if (optind == argc - 1) {
        fname = argv[optind];
    } else {
        fprintf(stderr, "[%s] error: unexpected argument \"%s\"\n", __func__,
                argv[optind + 1]);
        usage(EXIT_FAILURE);
    }

    acirc_init(&c);
    printf("reading circuit\n");
    acirc_parse(&c, fname);

    printf("circuit: ninputs=%lu nconsts=%lu noutputs=%lu ngates=%lu ntests=%lu nrefs=%lu\n",
           c.ninputs, c.nconsts, c.noutputs, c.ngates, c.ntests, c.nrefs);

    obf_params_init(&op, &c, chunker_in_order, rchunker_in_order, simple);

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
    secret_params_init(mmap, &st, &op, lambda, rng);
    public_params pp;
    public_params_init(mmap, &pp, &st);

    printf("obfuscating...\n");
    obfuscation *obf;
    obf = obfuscation_new(mmap, &pp);
    obfuscate(obf, &st, rng);

    puts("evaluating...");
    int res[c.noutputs];
    for (size_t i = 0; i < c.ntests; i++) {
        evaluate(mmap, res, c.testinps[i], obf, &pp);
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
    public_params_clear(mmap, &pp);
    secret_params_clear(mmap, &st);
    /* obf_params_clear(&op); */
    /* acirc_clear(&c); */

    return 0;
}
