#include <stdio.h>
#include "parse.tab.h"
extern int yyparse();
extern FILE *yyin;

int main( int argc, char **argv ){
    ++argv, --argc;
    if (argc > 0)
        yyin = fopen( argv[0], "r" );
    else
        yyin = stdin;
    int ntests = 0;
    yyparse(&ntests);
    printf("%d tests\n", ntests);
    return 0;
}
