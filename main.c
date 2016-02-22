#include <stdio.h>
#include <stdbool.h>
#include "circuit.h"
#include "parse.tab.h"
#include "clt13.h"

extern int yyparse();
extern FILE *yyin;

int main( int argc, char **argv ){
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
    printf("plaintext tests %d", ensure(&c));

    /*clt_state mmap;*/
    /*clt_setup(&mmap, kappa, lambda, nzs, pows, "", true);*/
    /*clt_state_clear(&mmap);*/
    
    circ_clear(&c);
    return 0;
}
