%code requires { #include "circuit.h" }

%{
#include "circuit.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int yylex();
void yyerror(circuit *c, const char *m){ printf("Error! %s\n", m); }
unsigned long from_bitstring (char *s);

#define YYINITDEPTH 100000

%}

%parse-param{ circuit *c }

%union { 
    unsigned long val;
    char *str;
    operation op;
};

%token <op> GATETYPE;
%token <str> NUM
%token <val> XID YID
%token TEST INPUT GATE OUTPUT

%%

prog: line prog | line

line: test | xinput | yinput | gate | output

test: TEST NUM NUM 
{ 
    circ_add_test(c, $2, $3); 
}

xinput: NUM INPUT XID 
{ 
    circ_add_xinput(c, atoi($1), $3);           
    free($1);
}

yinput: NUM INPUT YID NUM 
{ 
    circ_add_yinput(c, atoi($1), $3, atoi($4)); 
    free($1); 
    free($4); 
}

gate: NUM GATE GATETYPE NUM NUM 
{ 
    circ_add_gate(c, atoi($1), $3, atoi($4), atoi($5), false); 
    free($1);
    free($4);
    free($5);
}

output: NUM OUTPUT GATETYPE NUM NUM 
{ 
    circ_add_gate(c, atoi($1), $3, atoi($4), atoi($5), true); 
    free($1);
    free($4);
    free($5);
}
