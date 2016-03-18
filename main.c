#include "circuit.h"
#include "clt13.h"
#include "input_chunker.h"
#include "obfuscate.h"
#include "util.h"
#include "level.h"

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

int main( int argc, char **argv ){
    ++argv, --argc;
    if (argc > 0)
        yyin = fopen( argv[0], "r" );

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

    obf_params op;
    size_t nsyms = 4;
    obf_params_init(&op, &c, chunker_mod, nsyms);

    for (int i = 0; i < c.noutputs; i++) {
        printf("c=%lu o=%d type=", nsyms, i);
        print_array(op.types[i], nsyms + 1);
        puts("");
    }
    printf("M=%u\n", op.M);

    gmp_randstate_t rng;
    seed_rng(&rng);

    mpz_t *moduli = malloc((op.c+3) * sizeof(mpz_t));
    for (int i = 0; i < op.c+3; i++) {
        mpz_init(moduli[i]);
        mpz_urandomb(moduli[i], rng, 128);
    }

    fake_params fp;
    fake_params_init(&fp, &op, moduli);

    obfuscation obf;
    obfuscation_init(&obf, &fp);

    obfuscation_clear(&obf);

    for (int i = 0; i < op.c+3; i++) {
        mpz_clear(moduli[i]);
    }
    free(moduli);
    gmp_randclear(rng);
    fake_params_clear(&fp);
    obf_params_clear(&op);

    /*clt_state mmap;*/
    /*clt_setup(&mmap, get_kappa(&c), lambda + depth(&c, c.outref), nzs, tl, dir, true);*/
    /*obfuscate(&mmap, &c);*/
    /*clt_state_clear(&mmap);*/

    circ_clear(&c);
    return 0;
}
