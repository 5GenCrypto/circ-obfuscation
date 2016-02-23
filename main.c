#include <stdio.h>
#include <stdbool.h>
#include "circuit.h"
#include "obfuscate.h"
#include "parse.tab.h"
#include "clt13.h"

extern int yyparse();
extern FILE *yyin;
int extern g_verbose;

int main( int argc, char **argv ){
    g_verbose = 1;

    ++argv, --argc;
    if (argc > 0)
        yyin = fopen( argv[0], "r" );
    else
        yyin = stdin;

    circuit c;
    circ_init(&c);
    yyparse(&c);

    printf("circuit: ninputs=%d nconsts=%d ngates=%d ntests=%d\n",
            c.ninputs, c.nconsts, c.ngates, c.ntests);

    int* pows = malloc((c.ninputs + 1) * sizeof(int));
    get_pows(pows, &c);
    printf("circuit: ");
    print_array(pows, c.ninputs + 1);
    puts("");

    int nzs = num_indices(&c);
    int* tl = malloc(nzs * sizeof(int));
    get_top_level_index(tl, &c);
    printf("top-level index: ");
    print_array(tl, nzs);
    puts("");

    free(pows);
    free(tl);

    // TODO: top level index

    printf("plaintext tests %d\n", ensure(&c));
    /*clt_state mmap;*/
    /*clt_setup(&mmap, kappa, lambda, nzs, pows, "", true);*/
    /*clt_state_clear(&mmap);*/

    circ_clear(&c);
    return 0;
}
