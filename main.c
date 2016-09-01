#include "aesrand.h"
#include "mmap.h"
#include "evaluate.h"
#include "input_chunker.h"
#include "level.h"
#include "obfuscate.h"
#include "util.h"

#include <acirc.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// obfuscate
// save circuit w/o consts
// evaluate

int extern g_verbose;

int main (int argc, char **argv)
{
    size_t lambda = 16;

    ++argv, --argc;

    /*char dir[100];*/
    /*int lambda = 4;*/
    /*sprintf(dir, "%s.%d", argv[0], lambda);*/

    /*// make the obfuscation directory if it doesn't exist already*/
    /*struct stat st = {0};*/
    /*if (stat(dir, &st) == -1) mkdir(dir, 0700);*/

    acirc c;
    acirc_init(&c);
    printf("reading circuit\n");
    acirc_parse(&c, argv[0]);

    size_t symlen;
    if (argc >= 2)
        symlen = atoi(argv[1]);
    else
        symlen = 1;

    assert((c.ninputs % symlen) == 0);
    size_t nsyms = c.ninputs / symlen;

    bool input_is_rachel_representation = symlen > 1;

    // printf("calculating degree\n");
    // size_t d = acirc_max_degree(&c);

    // printf("circuit: ninputs=%lu nconsts=%lu ngates=%lu ntests=%lu nrefs=%lu degree=%lu\n",
    //                  c.ninputs, c.nconsts, c.ngates, c.ntests, c.nrefs, d);

    printf("circuit: ninputs=%lu nconsts=%lu ngates=%lu ntests=%lu nrefs=%lu\n",
                     c.ninputs, c.nconsts, c.ngates, c.ntests, c.nrefs);

    /*test_chunker(chunker_in_order, rchunker_in_order, nsyms, c.ninputs);*/
    /*test_chunker(chunker_mod, rchunker_mod, nsyms, c.ninputs);*/

    obf_params op;
    obf_params_init(&op, &c, chunker_in_order, rchunker_in_order, nsyms, input_is_rachel_representation);
    /*obf_params_init(&op, &c, chunker_mod, rchunker_mod, nsyms);*/

    printf("params: c=%lu ell=%lu q=%lu M=%lu D=%lu\n", op.c, op.ell, op.q, op.M, op.D);

    printf("consts: ");
    array_print(c.consts, c.nconsts);
    puts("");

    for (int i = 0; i < c.noutputs; i++) {
        printf("output bit %d: type=", i);
        array_print_ui(op.types[i], nsyms + 1);
        puts("");
    }

    acirc_ensure(&c, true);

    aes_randstate_t rng;
    aes_randinit(rng);

    printf("initializing params..\n");
    level *vzt = level_create_vzt(&op);
    secret_params st;
    secret_params_init(&st, &op, vzt, lambda, rng);


    printf("obfuscating...\n");
    obfuscation obf;
    obfuscation_init(&obf, &st);
    obfuscate(&obf, &st, rng, input_is_rachel_representation);

    // check obfuscation serialization
    /*FILE *obf_fp = fopen("test.lin", "w+");*/
    /*obfuscation_write(obf_fp, &obf);*/
    /*rewind(obf_fp);*/
    /*obfuscation obfp;*/
    /*obfuscation_read(&obfp, obf_fp, &op);*/
    /*obfuscation_eq(&obf, &obfp);*/
    /*obfuscation_clear(&obfp);*/
    /*fclose(obf_fp);*/

    public_params pp;
    public_params_init(&pp, &st);

    puts("evaluating...");
    /*int *res = lin_malloc(c.noutputs * sizeof(int));*/
    int res[c.noutputs];
    for (int i = 0; i < c.ntests; i++) {
        /*void evaluate (bool *rop, const bool *inps, obfuscation *obf, public_params *p);*/
        evaluate(res, c.testinps[i], &obf, &pp, input_is_rachel_representation);
        bool test_ok = ARRAY_EQ(res, c.testouts[i], c.noutputs);
        if (!test_ok)
            printf("\033[1;41m");
        printf("test %d input=", i);
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
    /*free(res);*/
    aes_randclear(rng);
    obfuscation_clear(&obf);
    public_params_clear(&pp);
    secret_params_clear(&st);
    obf_params_clear(&op);
    acirc_clear(&c);

    /*clt_state mmap;*/
    /*clt_setup(&mmap, get_kappa(&c), lambda + depth(&c, c.outref), nzs, tl, dir, true);*/
    /*obfuscate(&mmap, &c);*/
    /*clt_state_clear(&mmap);*/

    return 0;
}
