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

void
usage(int ret)
{
    printf("Usage: main [options] [circuit]\n");
    printf("Options:\n");
    printf("\t-f\tUse fake multilinear map\n");
    printf("\t-s\tSym length\n");
    exit(ret);
}

int
main(int argc, char **argv)
{
    size_t lambda = 16;
    char *fname;
    acirc c;
    size_t symlen = 1, nsyms;
    bool is_rachel;
    int arg;

    const mmap_vtable *mmap = &clt_vtable;

    while ((arg = getopt(argc, argv, "fs:")) != -1) {
        switch (arg) {
        case 'f':
            mmap = &dummy_vtable;
            break;
        case 's':
            symlen = atoi(optarg);
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

    if (c.ninputs % symlen != 0) {
        fprintf(stderr, "[%s] need ninputs %% symlen == 0\n", __func__);
        exit(EXIT_FAILURE);
    }
    nsyms = c.ninputs / symlen;

    is_rachel = symlen > 1;

    printf("circuit: ninputs=%lu nconsts=%lu ngates=%lu ntests=%lu nrefs=%lu\n",
                     c.ninputs, c.nconsts, c.ngates, c.ntests, c.nrefs);

    obf_params op;
    obf_params_init(&op, &c, chunker_in_order, rchunker_in_order, nsyms, is_rachel);

    printf("params: c=%lu ell=%lu q=%lu M=%lu D=%lu\n", op.c, op.ell, op.q, op.M, op.D);

    printf("consts: ");
    array_print(c.consts, c.nconsts);
    puts("");

    for (size_t i = 0; i < c.noutputs; i++) {
        printf("output bit %lu: type=", i);
        array_print_ui(op.types[i], nsyms + 1);
        puts("");
    }

    acirc_ensure(&c, true);

    aes_randstate_t rng;
    aes_randinit(rng);

    printf("initializing params..\n");
    level *vzt = level_create_vzt(&op);
    secret_params st;
    secret_params_init(mmap, &st, &op, vzt, lambda, rng);
    public_params pp;
    public_params_init(mmap, &pp, &st);

    printf("obfuscating...\n");
    obfuscation obf;
    obfuscation_init(mmap, &obf, &pp);
    obfuscate(&obf, &st, rng, is_rachel);

    // check obfuscation serialization
    /*FILE *obf_fp = fopen("test.lin", "w+");*/
    /*obfuscation_write(obf_fp, &obf);*/
    /*rewind(obf_fp);*/
    /*obfuscation obfp;*/
    /*obfuscation_read(&obfp, obf_fp, &op);*/
    /*obfuscation_eq(&obf, &obfp);*/
    /*obfuscation_clear(&obfp);*/
    /*fclose(obf_fp);*/


    puts("evaluating...");
    int res[c.noutputs];
    for (size_t i = 0; i < c.ntests; i++) {
        /*void evaluate (bool *rop, const bool *inps, obfuscation *obf, public_params *p);*/
        evaluate(mmap, res, c.testinps[i], &obf, &pp, is_rachel);
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
    obfuscation_clear(&obf);
    public_params_clear(mmap, &pp);
    secret_params_clear(mmap, &st);
    obf_params_clear(&op);
    acirc_clear(&c);

    return 0;
}
