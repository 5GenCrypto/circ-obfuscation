#include "circuit.h"
#include "util.h"
/*#include "clt13.h"*/
#include "evaluate.h"
#include "input_chunker.h"
#include "level.h"
#include "obfuscate.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

// obfuscate
// save circuit w/o consts
// evaluate

extern int yyparse();
extern FILE *yyin;
int extern g_verbose;

int main (int argc, char **argv)
{
    /*test_chunker(chunker_in_order, rchunker_in_order);*/
    /*test_chunker(chunker_mod, rchunker_mod);*/

    ++argv, --argc;
    if (argc >= 1)
        yyin = fopen( argv[0], "r" );

    size_t nsyms;
    if (argc >= 2)
        nsyms = atoi(argv[1]);
    else
        nsyms = 16;

    /*char dir[100];*/
    /*int lambda = 4;*/
    /*sprintf(dir, "%s.%d", argv[0], lambda);*/

    /*// make the obfuscation directory if it doesn't exist already*/
    /*struct stat st = {0};*/
    /*if (stat(dir, &st) == -1) mkdir(dir, 0700);*/

    g_verbose = 1;

    circuit c;
    circ_init(&c);
    yyparse(&c);

    printf("circuit: ninputs=%lu nconsts=%lu ngates=%lu ntests=%lu nrefs=%lu\n",
                     c.ninputs, c.nconsts, c.ngates, c.ntests, c.nrefs);
    ensure(&c);

    printf("consts: ");
    print_array(c.consts, c.nconsts);
    puts("");

    obf_params op;
    obf_params_init(&op, &c, chunker_in_order, rchunker_in_order, nsyms);
    /*obf_params_init(&op, &c, chunker_mod, rchunker_mod, nsyms);*/

    for (int i = 0; i < c.noutputs; i++) {
        printf("output bit %d: type=", i);
        print_array(op.types[i], nsyms + 1);
        puts("");
    }
    printf("params: c=%lu ell=%lu q=%lu M=%u\n", op.c, op.ell, op.q, op.M);

    gmp_randstate_t rng;
    seed_rng(&rng);

    mpz_t *moduli = lin_malloc((op.c+3) * sizeof(mpz_t));
    for (int i = 0; i < op.c+3; i++) {
        mpz_init(moduli[i]);
        mpz_urandomb(moduli[i], rng, 128);
    }

    printf("initializing params..\n");
    fake_params fp;
    fake_params_init(&fp, &op, moduli);

    printf("obfuscating...\n");
    obfuscation obf;
    obfuscation_init(&obf, &fp);

    obfuscate(&obf, &fp, &rng);

    printf("evaluating...\n");
    /*void evaluate (bool *rop, const bool *inps, obfuscation *obf, fake_params *p);*/
    int *res = lin_malloc(c.noutputs * sizeof(int));
    evaluate(res, c.testinps[0], &obf, &fp);

    // free all the things
    free(res);
    obfuscation_clear(&obf);
    for (int i = 0; i < op.c+3; i++) {
        mpz_clear(moduli[i]);
    }
    free(moduli);
    gmp_randclear(rng);
    fake_params_clear(&fp);
    obf_params_clear(&op);
    circ_clear(&c);

    /*clt_state mmap;*/
    /*clt_setup(&mmap, get_kappa(&c), lambda + depth(&c, c.outref), nzs, tl, dir, true);*/
    /*obfuscate(&mmap, &c);*/
    /*clt_state_clear(&mmap);*/

    return 0;
}
