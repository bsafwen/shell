#include "processing.h"
job		   *jobs = NULL ;
int		    foreground = 1  ;
char		   *dir ; 
int		    last_command_status = 0 ;
job *bg_jobs[MAX_JOBS] = {NULL};
int bg_index = 0 ;
int shell_is_interactive ;
pid_t shell_pgid ;
struct sigaction new ;
void initialize_shell(void)
{
    new.sa_flags = SA_SIGINFO ;
    new.sa_sigaction = sig_chld ;
    shell_is_interactive = isatty(STDIN_FILENO);
    if ( shell_is_interactive != 0 )
    {
	while ( tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()) )
	    kill(-shell_pgid, SIGTTIN);
	//These signals should be handled by the child not by the shell so we just ignore them
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	setpgid(0, 0); // Putting the shell in its own process group 
	tcsetpgrp(STDIN_FILENO, getpid()); // taking control of the terminal
	// use setrlimit to control th resources allocated for your program
    }
}

void launch_process(process *process_ptr, pid_t *pgid, int foreground)
{
    	
    pid_t pid ;
    pid = getpid() ;
    process_ptr->pid = pid ;
    if ( *pgid == 0 )
    {
	*pgid = pid ;
	if ( foreground )
	    tcsetpgrp(STDIN_FILENO, *pgid);
    }
    setpgid(pid, *pgid);
    execvp(process_ptr->argv[0], process_ptr->argv);

    exit(1);
}

void launch_job(job *job_ptr, int foreground)
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
	    pipe(pipes[l]);
	}
	for ( l = 0, p = iter->first_process ; p != NULL ; p = p->next_process, ++l )
	{
	    pid = fork() ;
	    if ( pid == 0 )
	    {
		if ( p->in != NULL )
		{
		    dup2(open(p->in, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR), STDIN_FILENO);
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
		launch_process(p, &iter->pgid, foreground);
	    }
	    else if ( pid < 0 )
	    {
		perror("fork ");
	    }
	    else
	    {
		if ( iter->pgid == 0 )
		    iter->pgid = pid ;
		tcsetpgrp(pid,STDOUT_FILENO);
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
	if ( foreground == 1 )
	    put_job_in_foreground(iter, 0);
	else
	    put_job_in_background(iter, 0);
    }
    signal(SIGCHLD, SIG_IGN);
}

void load_process(process **p, char *str, int foreground)
{
    process *temp = malloc(sizeof(process));
    temp->in = NULL ;
    temp->out = NULL ;
    temp->err = NULL ;
    temp->completed = 0 ;
    temp->stopped = 0 ;
    temp->next_process = NULL ;
    char *word = strtok(str, DELIMITERS);
    int   i = 1 ;
    if ( foreground == 0 )
	word = strtok(NULL, DELIMITERS);
    temp->argv = malloc(sizeof(char *)) ;
    temp->argv[0] = strdup(word);
    while ( (word = strtok(NULL, DELIMITERS)) != NULL )
    {
	if ((temp->argv = realloc(temp->argv, sizeof(char *) * (i+1))) == NULL )
	    perror("realloc ");
	temp->argv[i] = strdup(word);
	++i ;
    }
    if ( (temp->argv = realloc(temp->argv, sizeof(char *) * (i+1))) == NULL )
	perror("realloc ");
    temp->argv[i] = NULL ;
    if ( *p == NULL )
	*p = temp ;
    else
    {
	process *iter = *p ;
	while ( iter->next_process != NULL )
	    iter = iter->next_process ;
	iter->next_process = temp ;
    }
}

void load_job(job **j,process **p, char *str, int foreground )
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

void put_job_in_foreground(job *j, int cont)
{
    int i ;

    j->stopped = 0 ;
    j->foreground = 1 ;
    tcsetpgrp(STDIN_FILENO, j->pgid); // put the job in the foreground
    if ( cont == 1 )
    {
	if (kill(-j->pgid, SIGCONT) == -1)
	    perror("kill ");
    }
    for ( i = 0 ; i  < MAX_JOBS ; ++i )
	if (  bg_jobs[i] != NULL && bg_jobs[i]->foreground == 1 )
	    bg_jobs[i] = NULL ;
    wait_for_job(j);
    //put the shell back in the foreground
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

void put_job_in_background(job *j, int cont)
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

void fg(int i)
{
    if ( bg_jobs[i-1] != NULL && bg_jobs[i-1]->foreground == 0 )
	put_job_in_foreground(bg_jobs[i-1], 1);
    else
	printf("jobs error : invalid number.\n");
}

void bg(int i)
{
    if ( bg_jobs[i-1] != NULL )
	put_job_in_background(bg_jobs[i-1], 1);
    else
	printf("jobs error : invalid number.\n");
}

process * find_process(int pid)
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

int job_is_stopped(job *j)
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

int job_is_completed(job *j)
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

void sig_cont(int n, siginfo_t *info, void *foo)
{
    process *p = find_process(getpid());
    if ( p == NULL )
	;
    else
    {
	p->stopped = p->completed = 0 ;
    }
}

void print_job(job *j)
{
    job *iter = j ;
    process *p ;
    while ( iter != NULL )
    {
	p = iter->first_process ;
	while ( p != NULL )
	{
	    print_process(&p);
	    p = p->next_process ;
	}
	iter = iter->next_job ;
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

void kill_job(int signal ,int i)
{
    if (kill(-bg_jobs[i-1]->pgid,signal)==-1)
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
		iter->next_job = (iter->next_job)->next_job ;
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
	while ( p->argv[i] != NULL )
	{
	    free(p->argv[i]);
	    ++i ;
	}
	free(p->argv);
	p = p->next_process ;
	free(iter);
    }
}

void add_program(process **ptr, char *str, char *i, char *o, char *e)
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

void print_process(process **ptr)
{
    int i = 0 ; 
    if ( *ptr != NULL )
    {
	while ( (*ptr)->argv[i] != NULL )
	{
	    printf("%s\n",(*ptr)->argv[i]);
	    ++i;
	}
	if ( (*ptr)->in != NULL )
	    printf("%s\n",(*ptr)->in);
	if ( (*ptr)->out != NULL )
	    printf("%s\n",(*ptr)->out);
	if ( (*ptr)->err != NULL )
	    printf("%s\n",(*ptr)->err);
    }
}
