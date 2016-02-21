%{
#include <stdio.h>
extern FILE *yyin;
int yylex();
void yyerror(int *test, const char *m){ printf("Error! %d %s\n", *test, m); }
%}

%parse-param{ int *test }

%union { 
    int val;
    char *str;
};

%token <str> NUM 
%token <val> GATETYPE XID YID
%token TEST INPUT GATE OUTPUT

%%

prog: line prog | line

line: test | xinput | yinput | gate | output

test: TEST NUM NUM { puts("test"); *test += 1; }

xinput: NUM INPUT XID     { puts("xinput"); }
yinput: NUM INPUT YID NUM { puts("yinput"); }

gate:   NUM GATE   GATETYPE NUM NUM { puts("gate"); }
output: NUM OUTPUT GATETYPE NUM NUM { puts("output"); }

%%

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
