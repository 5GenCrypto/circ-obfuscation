#include "circuit.h"
#include "clt13.h"
#include "index.h"
#include "obfuscate.h"
#include "parse.tab.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// TODO: arguments parser for lambda & fresh
//       obfuscate
//       save circuit w/o consts
//       evaluate

extern int yyparse();
extern FILE *yyin;
int extern g_verbose;

int get_kappa(circuit* c);

int main( int argc, char **argv ){
    ++argv, --argc;
    if (argc > 0)
        yyin = fopen( argv[0], "r" );

    char dir[100];
    int lambda = 4;
    sprintf(dir, "%s.%d", argv[0], lambda);

    // make the obfuscation directory if it doesn't exist already
    struct stat st = {0};
    if (stat(dir, &st) == -1) mkdir(dir, 0700);

    g_verbose = 1;

    circuit c;
    circ_init(&c);
    yyparse(&c);

    printf("circuit: ninputs=%d nconsts=%d ngates=%d ntests=%d nrefs=%d\n",
                     c.ninputs, c.nconsts, c.ngates, c.ntests, c.nrefs);

    int* pows = malloc((c.ninputs + 1) * sizeof(int));
    get_pows(pows, &c);
    printf("circuit: ");
    print_array(pows, c.ninputs + 1);
    puts("");
    free(pows);

    /*int** levels = malloc(c.nrefs * sizeof(int*));*/
    /*int*  level_sizes = malloc(c.nrefs * sizeof(int));*/
    /*for (int i = 0; i < c.nrefs; i++)*/
        /*levels[i] = malloc(c.nrefs * sizeof(int));*/
    /*printf("topological levels:\n");*/
    /*int nlevels = topological_levels(levels, level_sizes, &c);*/
    /*for (int i = 0; i < nlevels; i++) {*/
        /*printf("\t");*/
        /*print_array(levels[i], level_sizes[i]);*/
        /*puts("");*/
    /*}*/
    /*for (int i = 0; i < c.nrefs; i++)*/
        /*free(levels[i]);*/
    /*free(levels);*/
    /*free(level_sizes);*/

    printf("plaintext tests %d\n", ensure(&c));

    int nzs = num_indices(&c);
    int* tl = malloc(nzs * sizeof(int));
    get_top_level_index(tl, &c);
    printf("top-level index: ");
    print_array(tl, nzs);
    puts("");

    clt_state mmap;
    clt_setup(&mmap, get_kappa(&c), lambda + depth(&c, c.outref), nzs, tl, dir, true);
    obfuscate(&mmap, &c);

    clt_state_clear(&mmap);
    free(tl);
    circ_clear(&c);
    return 0;
}

int get_kappa(circuit* c){
    return 4; // TODO: for testing
    int n = c->ninputs;
    int delta = ydegree(c, c->outref);
    for (int i = 0; i < n; i++) {
        delta += xdegree(c, c->outref, i);
    }
    return delta + 2*n + n*(2*n-1);
}
