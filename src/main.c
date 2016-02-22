#include <stdio.h>
#include "circuit.h"
#include "parse.tab.h"

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

    printf("circuit: ninputs=%d nconsts=%d ngates=%d\n", 
            c.ninputs, c.nconsts, c.ngates);
    return 0;
}
