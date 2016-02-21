%{

#include <stdio.h>
#include <stdbool.h>
#include "circuit.h"
#include "bitvector.h"

int yylex();
void yyerror(circuit *c, const char *m){ printf("Error! %s\n", m); }

unsigned long from_bitstring (char *s);

%}

%parse-param{ circuit *c }

%union { 
    unsigned long val;
    char *str;
};

%token <str> NUM 
%token <val> GATETYPE XID YID
%token TEST INPUT GATE OUTPUT

%%

prog: line prog | line

line: test | xinput | yinput | gate | output

test: TEST NUM NUM { 
    bitvector inp;
    bv_init(&inp, 1);
    bv_from_string(&inp, $2);
    circ_add_test(c, &inp, atoi($3));
}

xinput: NUM INPUT XID     { puts("xinput"); }
yinput: NUM INPUT YID NUM { puts("yinput"); }

gate:   NUM GATE   GATETYPE NUM NUM { puts("gate"); }
output: NUM OUTPUT GATETYPE NUM NUM { puts("output"); }
