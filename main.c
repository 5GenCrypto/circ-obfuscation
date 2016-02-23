#include <stdio.h>
#include <stdbool.h>
#include "circuit.h"
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

    int* xdegs = malloc(c.ninputs * sizeof(int));
    for (int i = 0; i < c.ninputs; i++)
        xdegs[i] = xdeg(&c, i);

    char* xdegstr = array2string(xdegs, c.ninputs);
    printf("circuit: xdegs=%s ydeg=%d\n", xdegstr, ydeg(&c));

    free(xdegstr);
    free(xdegs);

    // TODO: top level index

    printf("plaintext tests %d\n", ensure(&c));
    /*clt_state mmap;*/
    /*clt_setup(&mmap, kappa, lambda, nzs, pows, "", true);*/
    /*clt_state_clear(&mmap);*/
    
    circ_clear(&c);
    return 0;
}
