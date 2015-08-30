%{
#include "shell.tab.h"
extern char command[256];
%}
%%

[ \t]	    {strcat(command,yytext);yylloc.last_column++;
	     yylloc.first_line = yylloc.last_line;
	     yylloc.first_column = yylloc.last_column;}
\n  {++yylloc.last_line ; yylloc.last_column = 0 ;return EOL;}
"fg"	  |
"bg"	    {strcat(command,yytext);yylloc.last_column += 2 ;yylval.match = strdup(yytext); return BFG ;}
"cd"	    {strcat(command,yytext); yylloc.last_column += 2 ; return CD ;}
"run"	    {strcat(command, yytext); yylloc.last_column += 2 ; return RUN ; }
"jobs"	    {strcat(command,yytext);yylloc.last_column += 4 ; return JOBS ;}
"killjob"   {strcat(command,yytext);yylloc.last_column += 7 ; return KJOB ;}
"exit"	    {strcat(command,yytext);yylloc.last_column += 4 ;  return EXIT ;}
"(" |
"," |
")"			{strcat(command,yytext);yylloc.last_column += 1 ; return yytext[0] ;}
"pipe"			{strcat(command,yytext);yylloc.last_column += 4 ; return PIPE;}
"redirect"		{strcat(command,yytext);yylloc.last_column += 8 ; return REDIRECT ; }
[-\.a-zA-Z0-9_/{}@\;]+	{strcat(command,yytext);yylloc.last_column += strlen(yytext) ; yylval.match = strdup(yytext );return IDENTIFIER ; }
.	    {;}
%%
