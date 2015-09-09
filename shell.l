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
"print"	    {strcat(command, yytext) ; yylloc.last_column += 5 ; return PRINT ;}
"set"	    {strcat(command, yytext); yylloc.last_column += 3 ; return SET ;}
"unset"	    {strcat(command, yytext); yylloc.last_column += 5 ; return UNSET ;}
"setenv"    {strcat(command, yytext);yylloc.last_column += 6 ; return SETENV ;}
"unsetenv"    {strcat(command, yytext);yylloc.last_column += 8 ; return UNSETENV ;}
"(" |
"," |
")"			{strcat(command,yytext);yylloc.last_column += 1 ; return yytext[0] ;}
"pipe"			{strcat(command,yytext);yylloc.last_column += 4 ; return PIPE;}
"redirect"		{strcat(command,yytext);yylloc.last_column += 8 ; return REDIRECT ; }
[-\.a-zA-Z0-9_/{}@\;]+	{strcat(command,yytext);yylloc.last_column += strlen(yytext) ; yylval.match = strdup(yytext );return IDENTIFIER ; }
.	    {;}
%%
