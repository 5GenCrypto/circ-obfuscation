%{

#include <stdio.h>
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
