%{
#include <stdlib.h>
#include "circuit.tab.h"
%}

%option noyywrap

%%

:.*      { }
[ \t\n]+ { }
#        { }

[0-9]+      { yylval.str = strdup(yytext); return NUM;      } 
x[0-9]+     { yylval.val = atoi(yytext+1); return XID;      } 
y[0-9]+     { yylval.val = atoi(yytext+1); return YID;      } 
TEST        {                              return TEST;     } 
input       {                              return INPUT;    } 
gate        {                              return GATE;     } 
output      {                              return OUTPUT;   } 
ADD|SUB|MUL { yylval.str = strdup(yytext); return GATETYPE; } 
