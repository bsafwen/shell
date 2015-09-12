%{
#include "processing.h"
#include "prompt.h"
extern int		    yylex();
extern int		    yyerror(char *str);
extern int		    yyrestart(FILE *file);
extern int		    shell_pid ;
extern job		   *jobs ;
extern int		    last_command_status ; 
time_t			    rawTime ;
struct tm		   *tmp ;
char			    cwd[256];
char			    Time[10];
char			    prompt_length = 0 ;
int			    i = 0 ;
%}
%token REDIRECT PIPE PROGRAM BFG KJOB CD JOBS EOL IDENTIFIER EXIT RUN PRINT SETENV UNSETENV SET UNSET STATUS SHOW 
%union {
    char *match;
    struct process *proc;
    struct type_args *type_arg ;
}
%locations
%type <match> BFG
%type <match> IDENTIFIER 
%type <match> program
%type <proc> pipe redirect
%type <type_arg> builtin
%%

job	    :	
	    |		    job EOL { update_bg_jobs() ; update_jobs() ;command[0]='\0'; 
			    yylloc.last_column = prompt_length ;prompt(cwd,Time,&rawTime,&tmp);}
	    |		    job command EOL  	{launch_job(jobs);command[0]='\0'; update_bg_jobs() ;                                           update_jobs() ; 
			    yylloc.last_column = prompt_length ; prompt(cwd,Time,&rawTime,&tmp);}
	    |		    job builtin EOL {do_builtin($2);
			    i = 0 ;
			    while ( $2->args[i] != NULL )
				free($2->args[i++]);
			    free($2->args);
			    free($2);
			    update_bg_jobs() ; update_jobs() ;command[0]='\0'; 
			    yylloc.last_column = prompt_length ;prompt(cwd,Time,&rawTime,&tmp);}
	    |		    job EXIT EOL {return EXIT ;}
	    ;
command	    :		    redirect { load_job(&jobs, &$1, command, 1);}
	    |		    pipe { load_job(&jobs, &$1, command, 1);  }
	    |		    program { process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $1, NULL, NULL, NULL);
			    load_job(&jobs, &new, command, 1);
			    free($1);
			    }
	    
	    |		    RUN '(' program ')' { process *new = malloc(sizeof(process)) ;
			    new->next_process = NULL ;
			    add_program(&new, $3, NULL, NULL, NULL);
			    load_job(&jobs, &new, command, 0);
			    free($3);
			    }
	    ;
builtin	    :		    CD IDENTIFIER { type_args *new = malloc(sizeof(type_args));
			    new->type = CD ;
			    new->args = malloc(sizeof(char *)*2);
			    new->args[0] = strdup($2);
			    new->args[1] = NULL ;
			    free($2);
			    $$ = new ;
			    }
	    |		    CD            { type_args *new = malloc(sizeof(type_args));
			    new->type = CD ;
			    new->args = malloc(sizeof(char*));
			    new->args[0] = NULL ;
			    $$ = new ;
			    printf("CD\n");
			    }
	    |		    BFG IDENTIFIER { type_args *new = malloc(sizeof(type_args));
			    new->type = BFG ;
			    new->args = malloc(sizeof(char *)*3);
			    new->args[0] = strdup($1);
			    new->args[1] = strdup($2);
			    new->args[2] = NULL ;
			    free($1);
			    free($2);
			    $$ = new ; }
	    |		    KJOB IDENTIFIER IDENTIFIER { type_args *new = malloc(sizeof(type_args));
			    new->type = KJOB ;
			    new->args = malloc(sizeof(char *)*3) ;
			    new->args[0] = strdup($2);
			    new->args[1] = strdup($3);
			    new->args[2] = NULL ;
			    free($2);
			    free($3);
			    $$ = new ;
			    }
	    |		    JOBS { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = JOBS ;
			    $$ = new ;}
	    |		    PRINT IDENTIFIER { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = PRINT ;
			    new->args = malloc(sizeof(char *)*2);
			    new->args[0] = strdup($2);
			    new->args[1] = NULL ;
			    free($2);
			    $$ = new ;
			    }
	    |		    SET IDENTIFIER IDENTIFIER { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = SET ;
			    new->args = malloc(sizeof(char *)* 3);
			    new->args[0] = strdup($2); 
			    new->args[1] = strdup($3); 
			    new->args[2] = NULL ;
			    free($2);
			    free($3);
			    $$ = new ;
			    }
	    |		    UNSET IDENTIFIER { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = UNSET ;
			    new->args = malloc(sizeof(char *) * 2 );
			    new->args[0]  =  strdup($2);
			    new->args[1] = NULL ;
			    free($2);
			    $$ = new ;
			    }
	    |		    SETENV IDENTIFIER { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = SETENV ;
			    new->args = malloc(sizeof(char *) * 2 );
			    new->args[0]  =  strdup($2);
			    new->args[1] = NULL ;
			    free($2);
			    $$ = new ;
			    }
	    |		    UNSETENV IDENTIFIER { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = UNSETENV ;
			    new->args = malloc(sizeof(char *) * 2 );
			    new->args[0]  =  strdup($2);
			    new->args[1] = NULL ;
			    free($2);
			    $$ = new ;
			    }
	    |		    STATUS { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = STATUS ;
			    $$ = new ;
			    }
	    |		    SHOW { type_args *new = malloc(sizeof(type_args)) ; 
			    new->type = SHOW ;
			    $$ = new ;
			    }
	    ;
redirect    :		    REDIRECT '(' IDENTIFIER ',' IDENTIFIER ',' IDENTIFIER ',' program ')' { 
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $9, $3, $5, $7);
			    free($3);
			    free($5);
			    free($7);
			    free($9);
			    $$ = new ;
			    }
	    |		    REDIRECT '(' IDENTIFIER ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process=NULL ;
			    add_program(&new, $5, NULL, $3, NULL);
			    $$ = new ;
			    free($3);
			    free($5);
			    }
	    |		    REDIRECT '(' IDENTIFIER ',' IDENTIFIER ','  program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process=NULL ;
			    add_program(&new, $7, $3, $5, NULL);
			    $$ = new ;
			    free($3);
			    free($5);
			    free($7);
			    }
	    |		    REDIRECT '(' ',' IDENTIFIER ',' IDENTIFIER ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $8, NULL, $4, $6);
			    $$ = new ;
			    free($4);
			    free($6);
			    free($8);
			    }
	    |		    REDIRECT '(' ',' ',' IDENTIFIER ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $7, NULL , NULL , $5);
			    $$ = new ;
			    free($5);
			    free($7);
			    }
	    |		    REDIRECT '(' IDENTIFIER ',' ',' ',' program ')' {
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    add_program(&new, $7, $3 , NULL , NULL);
			    $$ = new ;
			    free($3);
			    free($7);
			    }
	    ;
program	    :		    program IDENTIFIER {
						 char *str=malloc(sizeof(char)*(2+strlen($1)+strlen($2)));
						 strcpy(str,$1);
						 strcat(str," ");
						 strcat(str,$2);   
						 free($2);
						 free($1);
						 $$ = str ;
						 }
	    |		    IDENTIFIER {$$ = strdup($1);free($1);}
	    ;
pipe	    :		    PIPE '(' program ',' program ')' { process *first = malloc(sizeof(process));
	 process *second = malloc(sizeof(process));
			    first->next_process = second ;
			    second->next_process = NULL ;
			    add_program(&first, $3, NULL, NULL, NULL);
			    add_program(&second, $5, NULL, NULL, NULL);
			    $$ = first ;
			    free($3);
			    free($5);
			    }
	    |		    PIPE '(' program ',' pipe ')' { process *first = malloc(sizeof(process));
			    first->next_process = $5 ;
			    add_program(&first, $3, NULL, NULL, NULL);
			    $$ = first ;
			    free($3);
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
			    free($5);
			    }
	    |		    PIPE '(' redirect ',' redirect ')' {
			    $3->next_process = $5 ;
			    $$ = $3 ;
			    }
	    |	    	    PIPE '(' pipe ',' pipe ')' { process *temp = $3 ;
			    while ( temp != NULL && temp->next_process != NULL )
			    {
				temp = temp->next_process ;
			    }
			    temp->next_process = $5 ;
			    $$ = $3 ;
			    }
	    |		    PIPE '(' redirect ',' pipe ')' { $3->next_process = $5 ; $$ = $3 ;}
	    |		    PIPE '(' pipe ',' redirect ')' {process *temp = $3 ;
			    while ( temp != NULL && temp->next_process != NULL )
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
			    free($3);
			    }
	    |		    PIPE '(' redirect ',' program  ')'	{
			    process *new = malloc(sizeof(process));
			    new->next_process = NULL ;
			    $3->next_process = new ;
			    add_program(&new, $5, NULL, NULL, NULL);
			    free($5);
			    $$ = $3 ; 
			    }
	    ;
%%

int main(int argc, char *argv[])
{
      initialize_shell();
    yylloc.first_line = yylloc.last_line = 1 ;
    prompt(cwd,Time,&rawTime,&tmp);
    yylloc.first_column = yylloc.last_column = prompt_length ;
    while ( yyparse() != EXIT )
    {
    }
    cleanup();
    return 0 ;
}

int yyerror(char *str)
{
    int i = 0 ;
    while ( ++i < (yylloc.last_column+2) )
	printf(" ");
    printf("^\n");
    printf("%s : unexpected token.",str);
    yyrestart(stdin);
    printf("\n");
    prompt(cwd,Time,&rawTime,&tmp);
    last_command_status = 1 ;
    yylloc.last_column = prompt_length ;
    return 1 ;
}
