%{
#include "processing.h"
extern job		   *jobs ;
extern int		    last_command_status ; 
%}
%token REDIRECT PIPE PROGRAM BFG KJOB CD JOBS EOL IDENTIFIER EXIT RUN 
%union {
    char *match;
    struct process *proc;
}
%locations
%type <match> BFG
%type <match> IDENTIFIER 
%type <match> program
%type <proc> pipe redirect
%%

job	    :	
	    |		    job command EOL  	{launch_job(jobs, 1);command[0]='\0'; update_bg_jobs() ; update_jobs() ; printf("> ");}
	    |		    job builtin EOL {update_bg_jobs() ; update_jobs() ;command[0]='\0';printf("> ");}
	    |		    job EXIT EOL {return EXIT ;}
	    ;
command	    :		    redirect { load_job(&jobs, &$1, command, 1);
			     }
	    |		    pipe { printf("pipe found!\n");load_job(&jobs, &$1, command, 1);  }
	    |		    program { process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $1, NULL, NULL, NULL);
			    load_job(&jobs, &new, command, 1);
			    }
	    ;
builtin	    :		    CD IDENTIFIER {  errno = 0 ;  if (   chdir($2) == -1  )
					     {
						last_command_status = errno ;
						perror("chdir ");
						return 1 ;
					     }
					     last_command_status = 0 ;
					  }

	    |		    CD            { char *home_dir = getenv("HOME");
					    if ( home_dir == NULL )
					    {
						last_command_status = 1 ;
						printf("Home directory not defined.\n");
						return 1 ;
					    }
					    chdir(home_dir);
					  }
	    |		    BFG IDENTIFIER { unsigned n = atoi($2);
			    if ( n == 0 )
			    {
				yyerror("invalid integer");
				last_command_status = 1 ;
				return 1 ;
			    }
			    else
			    {
				if ( strcmp($1, "bg") == 0 )
				    bg(n);
				else if ( strcmp($1, "fg") == 0 )
				    fg(n);
				last_command_status = 0 ;
			    }
			    }
	    |		    KJOB IDENTIFIER IDENTIFIER {  unsigned signal, id ;
			    signal = atoi($2);
			    if ( signal == 0 )
				yyerror("invalid integer");
			    id = atoi($3);
			    if ( id == 0 )
				yyerror("invalid integer");
			    kill_job(signal, id);}
	    |		    JOBS { print_bg_jobs() ; last_command_status = 0 ; }
	    |		    RUN '(' program ')' { process *new = malloc(sizeof(process)) ;
			    new->next_process = NULL ;
			    add_program(&new, $3, NULL, NULL, NULL);
			     load_job(&jobs, &new, command, 0);
			    launch_job(jobs, 0);
			     }
	    ;
redirect    :		    REDIRECT '(' IDENTIFIER ',' IDENTIFIER ',' IDENTIFIER ',' program ')' { 
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $9, $3, $5, $7);
			    $$ = new ;
			    }
	    |		    REDIRECT '(' IDENTIFIER ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process=NULL ;
			    add_program(&new, $5, NULL, $3, NULL);
			    $$ = new ;
			    }
	    |		    REDIRECT '(' IDENTIFIER ',' IDENTIFIER ','  program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process=NULL ;
			    add_program(&new, $7, $3, $5, NULL);
			    $$ = new ;
			    }
	    |		    REDIRECT '(' ',' IDENTIFIER ',' IDENTIFIER ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $8, NULL, $4, $6);
			    $$ = new ;
			    }
	    |		    REDIRECT '(' ',' ',' IDENTIFIER ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $7, NULL , NULL , $5);
			    $$ = new ;
			    }
	    |		    REDIRECT '(' IDENTIFIER ',' ',' ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $7, $3 , NULL , NULL);
			    $$ = new ;
			    }
	    ;
program	    :		    program IDENTIFIER { $$ = realloc($$, sizeof(char)*(1+strlen($$)+strlen($2)));
						 strcat($$," ");
						 strcat($$,$2);   }
	    |		    IDENTIFIER { $$ = strdup($1);}
	    ;
pipe	    :		    PIPE '(' program ',' program ')' { process *first = malloc(sizeof(process));
			    process *second = malloc(sizeof(process));
			    first->next_process = second ;
			    second->next_process = NULL ;
			    add_program(&first, $3, NULL, NULL, NULL);
			    add_program(&second, $5, NULL, NULL, NULL);
			    $$ = first ;
			    }
	    |		    PIPE '(' program ',' pipe ')' { process *first = malloc(sizeof(process));
			    first->next_process = $5 ;
			    add_program(&first, $3, NULL, NULL, NULL);
			    $$ = first ;
			    }
	    |		    PIPE '(' pipe ',' program  ')' { process *second = malloc(sizeof(process));
			    second->next_process = NULL ;
			    process *temp = $3 ;
			    while ( temp != NULL && temp->next_process != NULL )
			    {
				temp = temp->next_process ;
			    }
			    add_program(&second, $5, NULL, NULL, NULL);
			    temp->next_process = second; 
			    $$ = $3 ;
			    }
	    |		    PIPE '(' redirect ',' redirect ')' {
			    $3->next_process = $5 ;
			    $$ = $3 ;
			    }
	    |	    	    PIPE '(' pipe ',' pipe ')' { process *temp = $3 ;
			    while ( temp != NULL & temp->next_process != NULL )
			    {
				temp = temp->next_process ;
			    }
			    temp->next_process = $5 ;
			    $$ = $3 ;
			    }
	    |		    PIPE '(' redirect ',' pipe ')' { $3->next_process = $5 ; $$ = $3 ;}
	    |		    PIPE '(' pipe ',' redirect ')' {process *temp = $3 ;
			    while ( temp != NULL & temp->next_process != NULL )
			    {
				temp = temp->next_process ;
			    }
			    temp->next_process = $5 ;
			    $$ = $3 ;
			    }
	    |		    PIPE '(' program ',' redirect ')' { process *first = malloc(sizeof(process));
			    first->next_process = $5 ;
			    add_program(&first, $3, NULL, NULL, NULL);
			    $$ = first ;
			    }
	    |		    PIPE '(' redirect ',' program  ')'	{
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    $3->next_process = new ;
			    add_program(&new, $5, NULL, NULL, NULL);
			    $$ = $3 ; 
			    }
	    ;
%%

int main(int argc, char *argv[])
{
    initialize_shell();
    yylloc.first_line = yylloc.last_line = 1 ;
    yylloc.first_column = yylloc.last_column = 0 ;
    printf("> ");
    while ( yyparse() != EXIT )
    {
    }
}

int yyerror(char *str)
{
    int i = 0 ;
    while ( ++i < (yylloc.last_column+2) )
	printf(" ");
    printf("^\n");
    printf("%s : unexpected token.",str);
    yyrestart(stdin);
    yylloc.last_column = 0 ;
    printf("\n> ");
    last_command_status = 1 ;
}
