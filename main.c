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
    free(pows);

    int nzs = num_indices(&c);
    int* tl = malloc(nzs * sizeof(int));
    get_top_level_index(tl, &c);
    printf("top-level index: ");
    print_array(tl, nzs);
    puts("");
    free(tl);

    int* refs = calloc(c.nrefs, sizeof(int));
    topological_order(refs, &c);
    printf("topological order: ");
    print_array(refs, c.nrefs);
    puts("");
    free(refs);

    int** levels = malloc(c.nrefs * sizeof(int*));
    int*  level_sizes = malloc(c.nrefs * sizeof(int));
    for (int i = 0; i < c.nrefs; i++)
        levels[i] = malloc(c.nrefs * sizeof(int));

    printf("topological levels:\n");
    int nlevels = topological_levels(levels, level_sizes, &c);
    for (int i = 0; i < nlevels; i++) {
        printf("\t");
        print_array(levels[i], level_sizes[i]);
        puts("");
    }

    for (int i = 0; i < c.nrefs; i++)
        free(levels[i]);
    free(levels);
    free(level_sizes);

    // TODO: top level index

    printf("plaintext tests %d\n", ensure(&c));
    /*clt_state mmap;*/
    /*clt_setup(&mmap, kappa, lambda, nzs, pows, "", true);*/
    /*clt_state_clear(&mmap);*/

    circ_clear(&c);
    return 0;
}
