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
    return 0;
}
