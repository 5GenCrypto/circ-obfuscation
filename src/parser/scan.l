%{
#include "circuit.h"
#include "parse.tab.h"
%}

%option noyywrap
%option always-interactive

%%

[0-9]+      { yylval.str = strdup(yytext); return NUM;      } 
x[0-9]+     { yylval.val = atoi(yytext+1); return XID;      } 
y[0-9]+     { yylval.val = atoi(yytext+1); return YID;      } 
TEST        {                              return TEST;     } 
input       {                              return INPUT;    } 
gate        {                              return GATE;     } 
output      {                              return OUTPUT;   } 
ADD|SUB|MUL { yylval.op  = str2op(yytext); return GATETYPE; } 

:.*      { }
[ \t\n]+ { }
#        { }