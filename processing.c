#include "processing.h"
#define DELIM "@"
int		    shell_pid = 0 ;
job		   *jobs = NULL ;
int		    foreground = 1  ;
char		   *dir ; 
int		    last_command_status = 0 ;
job		   *bg_jobs[MAX_JOBS]={NULL} ;
int		     bg_index = 0 ;
int		    shell_is_interactive ;
pid_t		    shell_pgid ;
struct sigaction    new ;
variable	   *list_variables = NULL ;
extern char		  **environ ;
void initialize_shell(void)
{
    if ( shell_pid == 0 )
	shell_pid = getpid();
    setenv("MAXTIME","0",1);
    setenv("MAXJOBS","50",1);
    setenv("PROMPT","49@231@34%H%M%P",1);
    new.sa_flags = SA_SIGINFO ;
    new.sa_sigaction = sig_chld ;
    shell_is_interactive = isatty(STDIN_FILENO);
    if ( shell_is_interactive != 0 )
    {
	while ( tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()) )
	    kill(-shell_pgid, SIGTTIN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	setpgid(0, 0); 
	tcsetpgrp(STDIN_FILENO, getpid()); 
    }
}
void launch_process(process *process_ptr, pid_t *pgid, const char foreground)
{
    pid_t pid ;
    pid = getpid() ;
    process_ptr->pid = pid ;
    if ( *pgid == 0 )
    {
	*pgid = pid ;
    }
    setpgid(pid, *pgid);
    if( execvp(process_ptr->argv[0], process_ptr->argv) == -1 )
	printf("command not found.\n");
    exit(1);
}
void launch_job(job *job_ptr) 
{
    if ( job_ptr != NULL )
    {
	sigaction(SIGCHLD, &new, NULL);
	job *iter = job_ptr ;
	while ( iter->next_job != NULL )
	    iter = iter->next_job ;
	process *p = iter->first_process ;
	pid_t pid ;
	int numberOfPipes = 0 ;
	while ( p->next_process != NULL )
	{
	    ++numberOfPipes ;
	    p = p->next_process ;
	}
	int pipes[numberOfPipes][2];
	int l,x ;
	for ( l = 0 ; l < numberOfPipes ; ++l )
	{
	    if ( pipe(pipes[l]) == -1 )
	    {
		perror("pipe ");
	    }
	}
	for ( l = 0, p = iter->first_process ; p != NULL ; p = p->next_process, ++l )
	{
	    pid = fork() ;
	    if ( pid == 0 )
	    {
		if ( p->in != NULL )
		{
		    dup2(open(p->in, O_RDONLY), STDIN_FILENO);
		}
		if ( p->out != NULL )
		{
		    dup2(open(p->out, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR), STDOUT_FILENO);
		}
		if ( p->err != NULL )
		{
		    dup2(open(p->err, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR), STDERR_FILENO);
		}
		if ( l != 0 && p->next_process != NULL )
		{
		    dup2(pipes[l-1][0], STDIN_FILENO);
		    dup2(pipes[l][1], STDOUT_FILENO);
		    for ( x = 0 ; x < numberOfPipes ; ++x )
		    {
			int k = 0 ;
			for ( k = 0 ; k < 2 ; ++k )
			{
			    if ( (x==l-1 && k == 0) | (x==l && k==1))
				continue ;
			    close(pipes[x][k]);
			}
		    }
		}
		else if ( l == 0 )
		{
		    if ( iter->foreground == 1 )
			tcsetpgrp(getpid(), STDIN_FILENO);
		    dup2(pipes[0][1], STDOUT_FILENO);
		    for ( x = 0 ; x < numberOfPipes ; ++x )
		    {
			int k = 0 ;
			for ( k = 0 ; k < 2 ; ++k )
			{
			    if ( x==0 && k == 1)
				continue ;
			    close(pipes[x][k]);
			}
		    }    
		}
		else if ( p->next_process == NULL )
		{
		    dup2(pipes[numberOfPipes-1][0], STDIN_FILENO);
		    for ( x = 0 ; x < numberOfPipes ; ++x )
		    {
			int k = 0 ;
			for ( k = 0 ; k < 2 ; ++k )
			{
			    if ( x==numberOfPipes-1 && k == 0)
				continue ;
			    close(pipes[x][k]);
			}
		    }    
		}
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGTTOU, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		launch_process(p, &iter->pgid, iter->foreground);
	    }
	    else
	    {
		if ( l == 0 )
		    tcsetpgrp(pid,STDOUT_FILENO);
		if ( iter->pgid == 0 )
		    iter->pgid = pid ;
		p->pid = pid ;
		setpgid(pid, iter->pgid);
	    }
	}
	for ( l = 0 ; l < numberOfPipes ; ++l )
	{
	    int k = 0 ;
	    for ( k = 0 ; k < 2 ; ++k )
		close(pipes[l][k]);
	}
	if ( iter->foreground == 1 )
	    put_job_in_foreground(iter, 0);
	else
	    put_job_in_background(iter, 0);
    }
    signal(SIGCHLD, SIG_IGN);
}
void load_job(job **j,process *const *p, const char *str, const char foreground )
{
    job *temp = malloc(sizeof(job));
    temp->command = strdup(str);
    temp->foreground = foreground ;
    if ( temp == NULL )
	perror("load_job() : malloc ");
    temp->stopped = 0 ;
    temp->next_job = NULL ;
    temp->pgid = 0 ;
    temp->first_process = *p ;
    if ( *j == NULL )
	*j = temp ;
    else
    {
	job *iter = *j ;
	while ( iter->next_job != NULL )
	    iter = iter->next_job ;
	iter->next_job = temp ;
    }
}
void put_job_in_foreground(job *j, const char cont)
{
    int i ;
    j->stopped = 0 ;
    j->foreground = 1 ;
    tcsetpgrp(STDIN_FILENO, j->pgid); 
    if ( cont == 1 )
    {
	if (kill(-j->pgid, SIGCONT) == -1)
	    perror("kill ");
    }
    for ( i = 0 ; i  < MAX_JOBS ; ++i )
	if (  bg_jobs[i] != NULL && bg_jobs[i]->foreground == 1 )
	    bg_jobs[i] = NULL ;
    wait_for_job(j);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}
void put_job_in_background(job *j, const char cont)
{
    j->foreground = 0 ;
    if ( cont == 1 )
    {
	if ( kill ( -j->pgid, SIGCONT) == -1 )
	    perror("kill ");
	else
	    j->stopped = 0 ;
    }
    else if ( cont == 0 )
    {
	int i = 0 ;
	for ( i = 0 ; i  < MAX_JOBS ; ++i )
	    if (  bg_jobs[i] != NULL && job_is_completed(bg_jobs[i]) )
		bg_jobs[i] = NULL ;
	i = 0 ;
	while ( i < MAX_JOBS && bg_jobs[i] != NULL )
	    ++i ;
	if ( bg_jobs[i] == NULL )
	    bg_jobs[i] = j ;
	else
	    printf("max jobs reached!\n");
    }
}
void wait_for_job(job *j)
{
    pid_t pid ;
    int status ;
    do
    {
	pid =  waitpid(-j->pgid, &status, WCONTINUED | WUNTRACED );
	process *p = find_process(pid);
	if ( p == NULL )
	{
	    break ;
	}
	else
	{
	    last_command_status = WEXITSTATUS(status);
	    if ( WIFSIGNALED(status) )
	    {
		p->completed = 1 ;
	    }
	    else if ( WIFEXITED(status) )
	    {
		p->status = WTERMSIG(status);
		p->completed = 1 ;
	    }
	    else if ( WIFSTOPPED(status) )
	    {
		j->foreground = 0 ;
		p->stopped = 1 ;
		p->completed = 0 ;
		j->stopped = 1 ;
		int i = 0 ;
		for ( i = 0 ; i  < MAX_JOBS ; ++i )
		    if (  bg_jobs[i] != NULL && job_is_completed(bg_jobs[i]) )
			bg_jobs[i] = NULL ;
		i = 0 ;
		while ( i < MAX_JOBS && bg_jobs[i] != NULL )
		    ++i ;
		if ( bg_jobs[i] == NULL )
		{
		    bg_jobs[i] = j ;
		    printf("process \"%s\" has been suspended. index = %d\n",j->command,i+1);
		}
		else
		    printf("max jobs reached!\n");
	    }
	    else if ( WIFCONTINUED(status) )
	    {
		pid =  waitpid(-j->pgid, &status, WUNTRACED );
		process *p = find_process(pid);
		if ( WIFSIGNALED(status) )
		{
		    p->completed = 1 ;
		}
		else if ( WIFEXITED(status) )
		{
		    p->status = WTERMSIG(status);
		    p->completed = 1 ;
		}
		else if ( WIFSTOPPED(status) )
		{
		    p->stopped = 1 ;
		    p->completed = 0 ;
		    j->stopped = 1 ;
		    j->foreground = 0 ;
		    int i = 0 ;
		    for ( i = 0 ; i  < MAX_JOBS ; ++i )
			if (  bg_jobs[i] != NULL && job_is_completed(bg_jobs[i]) )
			    bg_jobs[i] = NULL ;
		    i = 0 ;
		    while ( i < MAX_JOBS && bg_jobs[i] != NULL )
			++i ;
		    if ( bg_jobs[i] == NULL )
		    {
			printf("process \"%s\" has been suspended. index = %d\n",j->command,i+1);
			bg_jobs[i] = j ;
		    }
		    else
			printf("max jobs reached!\n");
		}
	    }
	}
    }
    while ( !job_is_stopped(j) && !job_is_completed(j));
}
void fg(const int i)
{
    if ( bg_jobs[i-1] != NULL && bg_jobs[i-1]->foreground == 0 )
	put_job_in_foreground(bg_jobs[i-1], 1);
    else
	printf("jobs error : invalid number.\n");
}
void bg(const int i)
{
    if ( bg_jobs[i-1] != NULL )
	put_job_in_background(bg_jobs[i-1], 1);
    else
	printf("jobs error : invalid number.\n");
}
process * find_process(const int pid)
{
    job *tempJ = jobs ; 
    process *temp = NULL ; ;
    while ( tempJ != NULL )
    {
	temp = tempJ->first_process ;
	while ( temp != NULL )
	{
	    if ( temp->pid == pid )
		return temp ;
	    temp = temp->next_process ;
	}
	tempJ = tempJ->next_job ;
    }
    return temp; 
}
int job_is_stopped(const job *j)
{
    process *p = j->first_process ;
    while ( p != NULL )
    {
	if ( p->stopped == 0 )
	    return 0 ;
	p = p->next_process ;
    }
    return 1 ;
}
int job_is_completed(const job *j)
{
    process *p = j->first_process ;
    while ( p != NULL )
    {
	if ( p->completed == 0 )
	    return 0 ;
	p = p->next_process ;
    }
    return 1 ;
}
void sig_chld(int n, siginfo_t *info , void *foo)
{
    process *p = find_process(info->si_pid) ;
    if ( p == NULL )
	;
    else
    {
	if ( info->si_signo == SIGTSTP )
	{
	    p->stopped = 1 ;
	    p->completed = 0 ;
	}
	else if ( info->si_signo == SIGINT )
	{
	    p->stopped = 1 ;
	    p->completed = 1 ;
	}
    }
}
void print_bg_jobs(void)
{
    int i ;
    for ( i = 0 ; i < MAX_JOBS ; ++i )
    {
	if ( bg_jobs[i] != NULL && bg_jobs[i]->foreground == 0 && (kill(bg_jobs[i]->pgid,0) != -1))
	    printf("%d) %s\n",i+1, bg_jobs[i]->command);
    }
}
void update_bg_jobs(void)
{
    int i ;
    for ( i = 0 ; i < MAX_JOBS ; ++i )
    {
	if ( bg_jobs[i] != NULL )
	{
	    if ( kill(-bg_jobs[i]->pgid, 0) == -1 )
	    {
		if (  bg_jobs[i]->foreground == 0 )
		    printf("%s : done\n",bg_jobs[i]->command);
		bg_jobs[i] = NULL ;
	    }
	}
    }
}
void kill_job(const char signal ,const int i)
{
    if (bg_jobs[i-1] != NULL && kill(-bg_jobs[i-1]->pgid,signal)==-1)
    {
	last_command_status = errno ;
	perror("kill ");
    }
    last_command_status = 0 ;
}
void update_jobs(void)
{
    if ( jobs != NULL && job_is_completed(jobs) )
    {
	job *delete_me = jobs ;
	jobs = jobs->next_job ;
	free_job(delete_me);
    }
    else
    {
	job *iter = jobs ;
	while ( iter != NULL )
	{
	    while (  iter->next_job != NULL  && ! job_is_completed(iter->next_job) )
	    {
		iter = iter->next_job; 
	    }
	    if ( iter->next_job != NULL )
	    {
		job *delete_me = iter->next_job ;
		iter->next_job = delete_me->next_job ;
		free_job(delete_me);
	    }
	    iter = iter->next_job ;
	}
    }
}
void free_job(job *j)
{
    free(j->command);
    delete_list(j->first_process);
    free(j);
}
void delete_list(process *p)
{
    process *iter = p ;
    int i = 0 ;
    while ( p != NULL )
    {
	iter = p ;
	i = 0 ;
	while ( p->argv[i] != NULL )
	{
	    free(p->argv[i]);
	    ++i ;
	}
	free(p->argv);
	free(p->in);
	free(p->out);
	free(p->err);
	p = p->next_process ;
	free(iter);
    }
}
void add_program(process **ptr,char *str,const char *i,const char *o,const char *e)
{
    (*ptr)->completed = 0 ;
    (*ptr)->stopped = 0 ;
    char c ;
    int numberOfArgs = 1 ;
    int k = 0 ;
    while ( (c = str[k]) != '\0') 
    {
	if ( isspace(c) )
	    ++numberOfArgs ;
	++k ;
    }
    (*ptr)->argv = malloc(sizeof(char *)*(numberOfArgs+1));
    char *word = strtok(str, " ");
    (*ptr)->argv[0] = strdup(word);
    int j = 1 ;
    while ( (word = strtok(NULL, " ")) != NULL )
    {
	(*ptr)->argv[j] = strdup(word);
	++j ;
    }
    (*ptr)->argv[j] = NULL ;
    if ( i != NULL )
	(*ptr)->in = strdup(i);
    else
	(*ptr)->in = NULL;
    if ( o != NULL )
	(*ptr)->out = strdup(o);
    else
	(*ptr)->out = NULL ;
    if ( e != NULL )
	(*ptr)->err = strdup(e);
    else
	(*ptr)->err = NULL ;
}
void set(const char *name, const char *value)
{
    variable *new = malloc(sizeof(variable));
    new->name = strdup(name);
    new->value = strdup(value);
    new->next_variable = NULL ;
    new->exported = 0 ;
    if ( list_variables == NULL )
	list_variables = new ;
    else
    {
	new->next_variable = list_variables; 
	list_variables = new ;
    }
}
void unset(const char *name)
{
    unsetenv(name);
    if( list_variables != NULL )
    {
	variable *temp = list_variables ;
	variable *previous = NULL ;
	while ( temp != NULL && strcmp(temp->name, name) != 0 )
	{
	    previous = temp ;
	    temp = temp->next_variable ;
	}
	if ( temp != NULL )
	{
	    variable *to_delete = NULL ;
	    if ( temp == list_variables )
	    {
		to_delete = list_variables ;
		list_variables = list_variables->next_variable ;
		free(to_delete->name);
		free(to_delete->value);
		free(to_delete);
	    }
	    else
	    {
		to_delete = previous->next_variable ;
		previous->next_variable = to_delete->next_variable; 
		free(to_delete->name);
		free(to_delete->value);
		free(to_delete);
	    }
	}
    }
}
void setENV(const char *name)
{
    variable *temp = list_variables ;
    while ( temp != NULL && strcmp(temp->name, name) != 0 )
    {
	temp = temp->next_variable ;
    }
    if ( temp != NULL )
    {
	setenv(temp->name, temp->value, 1);
    }
}
void unsetENV(const char *name)
{
    unsetenv(name);
}
char *lookup(const char *name)
{
    variable *temp = list_variables ;
    while ( temp != NULL && strcmp(temp->name, name) != 0 )
    {
	temp = temp->next_variable ;
    }
    return temp != NULL ? temp->value : getenv(name) ;
}
void cleanup(void)
{
    job *temp = jobs ;
    while ( temp != NULL )
    {
	kill(temp->pgid, SIGKILL);
	jobs = jobs->next_job ;
	free_job(temp);
	temp = jobs ;
    }
    variable *tempV = list_variables ;
    while ( tempV != NULL  )
    {
	list_variables = list_variables->next_variable ;
	free(tempV->name);
	free(tempV->value);
	free(tempV);
	tempV = list_variables ;
    }
}
int do_builtin(type_args *command)
{
    switch(command->type)
    {
	case CD:
	    if ( command->args[0] != NULL )
	    {
		errno = 0 ; 
		if (chdir(command->args[0]) == -1)  {
		    last_command_status = errno ;
		    perror("chdir ");
		    break;
		}
		last_command_status = 0 ;
	    }
	    else
	    {
		char *home_dir = getenv("HOME");
		if ( home_dir == NULL )
		{
		    last_command_status = 1 ;
		    printf("Home directory not defined.\n");
		    break;
		}
		if ( chdir(home_dir) == -1 )
		    perror("chdir :");
	    }
	    break ;
	case BFG:
	    {
		unsigned n = atoi(command->args[1]);
		if ( n == 0 )
		{
		    yyerror("invalid integer");
		    last_command_status = 1 ;
		    break;
		}
		else
		{
		    if ( strcmp(command->args[0], "bg") == 0 )
			bg(n);
		    else if ( strcmp(command->args[0], "fg") == 0 )
			fg(n);
		    last_command_status = 0 ;
		}
	    }
	    break ;
	case KJOB:
	    {
		unsigned signal, id ;
		signal = atoi(command->args[0]);
		if ( signal == 0 )
		    yyerror("invalid integer");
		else
		{
		    id = atoi(command->args[1]);
		    if ( id == 0 )
			yyerror("invalid integer");
		    else
			kill_job(signal, id);
		}
	    }
	    break ;
	case JOBS:
	    print_bg_jobs() ;
	    last_command_status = 0 ;
	    break;
	case PRINT:
	    {
		char *result = lookup(command->args[0]);
		if ( result != NULL )
		    printf("%s",result);
		printf("\n");
		last_command_status = 0 ;
	    }
	    break ;
	case SET:
	    set(command->args[0], command->args[1]);
	    last_command_status = 0 ;
	    break ;
	case UNSET:
	    unset(command->args[0]);
	    last_command_status = 0 ;
	    break ;
	case SETENV:
	    setENV(command->args[0]);
	    last_command_status = 0 ;
	    break ;
	case UNSETENV:
	    unsetENV(command->args[0]);
	    last_command_status = 0 ;
	    break;
	case STATUS:
	    printf("%d\n",last_command_status);
	    last_command_status = 0 ;
	    break ;
	case SHOW:
	    {
		char **temp = environ ;
		while ( temp &&  *temp != NULL )
		{
		    printf("%s\n",*temp);
		    ++temp ;
		}
	    }
	    break ;
    }
}
