// problem with find_process
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#define DELIMITERS " (),\n"
#define MAX_JOBS 50

static int bg_index = 0 ;
int shell_is_interactive ;
pid_t shell_pgid ;
struct sigaction new ;

typedef struct process
{
    struct process *next_process ;
    pid_t	    pid ;
    char	  **argv ;
    char	    completed ;
    char	    stopped ;
    int		    status ;
    int		    in, out, err ;
} process ;

typedef struct job
{
    struct job     *next_job ;
    char	   *command ;
    process	   *first_process ;
    pid_t	    pgid ;
    char	    stopped ;
    char	    foreground ;
} job ;

void update_jobs(void);
void delete_list(process *p);
void free_job(job *j);
void print_process(process *p);
void print_job(job *j);
job *bg_jobs[MAX_JOBS] = {NULL};

void kill_job(char *command);
void update_bg_jobs(void);
void sig_cont(int n, siginfo_t *info, void *foo);
void print_bg_jobs(void);
void sig_chld(int n, siginfo_t *info , void *foo);
int job_is_stopped(job *j);
int job_is_completed(job *j);
process * find_process(int pid);
void bg(char *command);
void fg(char *command);
void initialize_shell(void);
void load_process(process **p, char *str, int fg);
void load_job(job **j, char *str, int foreground);
void launch_process(process *p, pid_t *pid, int fg);
void launch_job(job *j, int fg);
void update_jobs(void);
void put_job_in_foreground(job *j, int cont);
void put_job_in_background(job *j, int cont);
void wait_for_job(job *j);
void report_jobs_status(void);
void redirect(char *command_line);


job		   *jobs = NULL ;
int		    foreground = 1  ;
char		   *dir ; 
int		    last_command_status = 0 ;
int main(void)
{

    char	    command_line[256] ;
    initialize_shell();
    while ( 1 )
    {
	printf("> ");
	if ( fgets(command_line, 256, stdin) != NULL )
	{

	    if ( command_line[0] != '\n' )
	    {
		if ( command_line[strlen(command_line)-1] == '\n' )
		    command_line[strlen(command_line)-1] = '\0' ;
		if ( strncmp(command_line, "cd ",3) == 0 )
		{
		    char *backup = malloc(sizeof(char)*(strlen(command_line)+1));
		    strcpy(backup, command_line);
		    dir = strtok(backup, DELIMITERS);
		    dir = strtok(NULL, DELIMITERS);
		    if (   chdir(dir) == -1  )
		    {
			free(backup);
			perror("chdir ");
			last_command_status = -1 ;
			continue ;
		    }
			free(backup);
		}

		else if ( strncmp(command_line, "exit",4) == 0 )
		    break ;
		else if ( strncmp(command_line, "redirect",8) == 0 )
		    redirect(command_line);
		else if ( strncmp(command_line, "run",3) == 0 )
		{
		    load_job(&jobs, command_line, 0 );
		    launch_job(jobs, 0);
		}
		else if ( strncmp(command_line, "fg",2) == 0 )
		    fg(command_line);
		else if ( strncmp(command_line, "bg", 2) == 0 )
		    bg(command_line);
		else if ( strncmp(command_line, "jobs",4) == 0 )
		{
		    print_bg_jobs();
		    last_command_status = 0 ;
		}
		else if ( strncmp(command_line, "killjob ", 8) == 0 )
		    kill_job(command_line);
		else if ( strncmp(command_line, "status", 6) == 0 )
		{
		    printf("%d\n",last_command_status);
		    last_command_status = 0 ;
		}
		else
		{
		    load_job(&jobs, command_line, 1 );
		    launch_job(jobs, 1);
		}
	    }
	}
	update_bg_jobs();
	/* report_jobs_status(); */
	update_jobs();
    }
   
    return 0 ;
}

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
    if ( shell_is_interactive )
    {
	pid_t pid ;
	pid = getpid() ;
	process_ptr->pid = pid ;
	if ( *pgid == 0 )
	    *pgid = pid ;
	setpgid(pid, *pgid);
	if ( foreground )
	    tcsetpgrp(STDIN_FILENO, *pgid);
	execvp(process_ptr->argv[0], process_ptr->argv);

	exit(1);
    }
}

void launch_job(job *job_ptr, int foreground)
{
    if ( job_ptr != NULL )
    {
	sigaction(SIGCHLD, &new, NULL);
	job *iter = job_ptr ;
	while ( iter->next_job != NULL )
	    iter = iter->next_job ;
	process *p ;
	pid_t pid ;
	for ( p = iter->first_process ; p != NULL ; p = p->next_process )
	{
	    pid = fork() ;
	    if ( pid == 0 )
	    {
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
		p->pid = pid ;
		if ( shell_is_interactive )
		{
		    if ( iter->pgid == 0 )
			iter->pgid = pid ;
		    setpgid(pid, iter->pgid);
		}
	    }
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
    temp->completed = 0 ;
    temp->stopped = 0 ;
    temp->next_process = NULL ;
    char *word = strtok(str, DELIMITERS);
    int   i = 1 ;
    if ( foreground == 0 )
	word = strtok(NULL, DELIMITERS);
    temp->argv = malloc(sizeof(char *)) ;
    temp->argv[0] = malloc((1+strlen(word))*sizeof(char));
    strcpy(temp->argv[0], word);
    while ( (word = strtok(NULL, DELIMITERS)) != NULL )
    {
	if ((temp->argv = realloc(temp->argv, sizeof(char *) * (i+1))) == NULL )
	    perror("realloc ");
	temp->argv[i] = malloc(strlen(word)*sizeof(char)+1);
	strcpy(temp->argv[i], word);
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

void load_job(job **j,char *str, int foreground )
{
    job *temp = malloc(sizeof(job));
    temp->command = malloc((strlen(str)+1)*sizeof(char));
    temp->foreground = foreground ;
    strcpy(temp->command, str);
    process *p = NULL ;
    load_process(&p, str, foreground);
    if ( temp == NULL )
	perror("load_job() : malloc ");
    temp->stopped = 0 ;
    temp->next_job = NULL ;
    temp->pgid = 0 ;
    temp->first_process = p ;
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
		    printf("prcess \"%s\" has been suspended. index = %d\n",j->command,i+1);
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
			printf("prcess \"%s\" has been suspended. index = %d\n",j->command,i+1);
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

void fg(char *command)
{
    char *number = strtok(command, DELIMITERS);
    number = strtok(NULL, DELIMITERS);
    int i = atoi(number);
    if ( bg_jobs[i-1] != NULL && bg_jobs[i-1]->foreground == 0 )
	put_job_in_foreground(bg_jobs[i-1], 1);
    else
	printf("jobs error : invalid number.\n");
}

void bg(char *command)
{
    char *number = strtok(command, DELIMITERS);
    number = strtok(NULL, DELIMITERS);
    int i = atoi(number);
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

void print_process(process *p)
{

    printf("pid = %d, completed = %d , stopped = %d\n",p->pid, p->completed, p->stopped);
    printf("%s\n",p->argv[0]);
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
	    print_process(p);
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

void kill_job(char *command)
{
    char *number = strtok(command, DELIMITERS);
    number = strtok(NULL, DELIMITERS);
    int signal = atoi(number);
    number = strtok(NULL, DELIMITERS);
    int  i = atoi(number);
    if (kill(-bg_jobs[i-1]->pgid,signal)==-1)
	perror("kill ");
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

/* void report_jobs_status(void) */
/* { */
/*     int j ; */
/*     job *temp = NULL ; */
/*     int status ; */
/*     pid_t pid = waitid(P_ALL, 0, NULL, WEXITED ); */
/*     process *p = find_process(pid); */
/*     if ( p == NULL ) */
/*     { */
/* 	printf("reporting : nothing to report!\n"); */
/*     } */
/*     else */
/*     { */
/* 	last_command_status = WEXITSTATUS(status); */
/* 	if ( WIFSIGNALED(status) ) */
/* 	{ */
/* 	    p->completed = 1 ; */
/* 	} */
/* 	else if ( WIFEXITED(status) ) */
/* 	{ */
/* 	    p->status = WTERMSIG(status); */
/* 	    p->completed = 1 ; */
/* 	} */
/* 	else if ( WIFSTOPPED(status) ) */
/* 	{ */
/* 	    p->stopped = 1 ; */
/* 	    p->completed = 0 ; */
/* 	} */

/*     } */
/*     for ( j = 0 ; j < MAX_JOBS ; ++j ) */
/*     { */
/* 	if ( bg_jobs[j] != NULL ) */
/* 	{ */
/* 	    if ( bg_jobs[j]->foreground == 0 && job_is_completed(bg_jobs[j]) ) */
/* 		    printf("%s : done\n",bg_jobs[j]->command); */
/* 		bg_jobs[j] = NULL ; */
/* 	} */
/*     } */
/* } */
