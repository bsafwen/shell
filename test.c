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
} process ;

typedef struct job
{
    struct job     *next_job ;
    char	   *command ;
    process	   *first_process ;
    pid_t	    pgid ;
    char	    stopped ;
    /* int		    sstdin, sstdout, sstderr ; */
} job ;

void print_process(process *p);
void print_job(job *j);
job *bg_jobs[MAX_JOBS] = {NULL};

void sig_cont(int n, siginfo_t *info, void *foo);
void print_bg_jobs(job *j);
void sig_chld(int n, siginfo_t *info , void *foo);
int mark_process_status(pid_t pid, int status);
int job_is_stopped(job *j);
int job_is_completed(job *j);
process * find_process(int pid);
void bg(job *j, char *command);
void fg(char *command);
void initialize_shell(void);
void load_process(process **p, char *str, int fg);
void load_job(job **j, char *str, int foreground);
void launch_process(process *p, pid_t *pid, int fg);
void launch_job(job *j, int fg);
void put_job_in_foreground(job *j, int cont);
void put_job_in_background(job *j, int cont);
void wait_for_job(job *j);


job		   *jobs = NULL ;
int		    foreground = 1  ;
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
		if ( strncmp(command_line, "exit",4) == 0 )
		    break ;
		if ( strncmp(command_line, "run",3) == 0 )
		{
		    load_job(&jobs, command_line, 0 );
		    launch_job(jobs, 0);
		}
		else if ( strncmp(command_line, "fg",2) == 0 )
		    fg(command_line);
		else if ( strncmp(command_line, "bg", 2) == 0 )
		    bg(jobs, command_line);
		else if ( strncmp(command_line, "jobs",4) == 0 )
		    print_bg_jobs(jobs);
		else
		{
		    load_job(&jobs, command_line, 1 );
		    launch_job(jobs, 1);
		}
	    }
	}
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
    else
	printf("no jobs to run.\n");
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
    j->stopped = 0 ;
    tcsetpgrp(STDIN_FILENO, j->pgid); // put the job in the foreground
    if ( cont == 1 )
    {
	if (kill(-j->pgid, SIGCONT) == -1)
	    perror("kill ");
    }
    wait_for_job(j);
    //put the shell back in the foreground
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

void put_job_in_background(job *j, int cont)
{
    j->stopped = 1 ;
    if ( cont == 1 )
    {
	if ( kill ( -j->pgid, SIGCONT) == -1 )
	    perror("kill ");
	else
	    j->stopped = 0 ;
    }
}

void wait_for_job(job *j)
{

    pid_t pid ;
    int status ;
    do
    {
	print_job(j);
	pid =  waitpid(-j->pgid, &status, WCONTINUED | WUNTRACED );
	process *p = find_process(pid);
	if ( p == NULL )
	{
	    printf("process  %d not found!\n", pid);
	    break ;
	}
	else
	{
	    if ( WIFSIGNALED(status) )
	    {
		printf("%d has been signaled.\n",pid);
		printf("signal = %d\n",WTERMSIG(status));
		p->completed = 1 ;
	    }
	    else if ( WIFEXITED(status) )
	    {
		printf("%d has exited.\n",pid);
		printf("exit status  = %d\n",WEXITSTATUS(status));
		p->status = WTERMSIG(status);
		p->completed = 1 ;
	    }
	    else if ( WIFSTOPPED(status) )
	    {
		printf("%d has been stopped.\n",pid);
		printf("signal  = %d\n",WSTOPSIG(status));
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
		    bg_jobs[i] = j ;
		else
		    printf("max jobs reached!\n");
	    }
	    else if ( WIFCONTINUED(status) )
	    {
		pid =  waitpid(-j->pgid, &status, WUNTRACED );
		process *p = find_process(pid);
		if ( WIFSIGNALED(status) )
		{
		    printf("%d has been signaled.\n",pid);
		    printf("signal = %d\n",WTERMSIG(status));
		    p->completed = 1 ;
		}
		else if ( WIFEXITED(status) )
		{
		    printf("%d has exited.\n",pid);
		    printf("exit status  = %d\n",WEXITSTATUS(status));
		    p->status = WTERMSIG(status);
		    p->completed = 1 ;
		}
		else if ( WIFSTOPPED(status) )
		{
		    printf("%d has been stopped.\n",pid);
		    printf("signal  = %d\n",WSTOPSIG(status));
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
			bg_jobs[i] = j ;
		    else
			printf("max jobs reached!\n");
		}
	    }
	}
	print_job(j);
    }
    while ( !job_is_stopped(j) && !job_is_completed(j));
    printf("wait_for_job done.\n");
}

void fg(char *command)
{
    char *number = strtok(command, DELIMITERS);
    number = strtok(NULL, DELIMITERS);
    int i = atoi(number);
    /* printf("%d\n",i); */
    if ( bg_jobs[i-1] != NULL )
	put_job_in_foreground(bg_jobs[i-1], 1);
    else
	printf("jobs error : invalid number.\n");
}

void bg(job *j , char *command)
{
    if ( j != NULL )
    {
	char *number = strtok(command, DELIMITERS);
	number = strtok(NULL, DELIMITERS);
	int i = atoi(number);
	printf("%d\n",i);
	int k = 1 ;
	job *iter = j ;
	while ( k < i && iter != NULL )
	{
	    ++k ;
	    iter= iter->next_job ;
	}
	if ( iter == NULL )
	    printf("bg : invalid number.\n");
	else
	    put_job_in_background(iter, 1);
    }
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

int mark_process_status(pid_t pid, int status)
{
    job *j ;
    process *p ;
    if ( pid > 0 )
    {
	for ( j = jobs ; j != NULL ; j = j->next_job )
	    for ( p = j->first_process ; p != NULL ; p = p->next_process)
		if ( pid == p->pid )
		    p->status = status ;
	if ( WIFSTOPPED(status) )
	{
	    p->stopped = 1 ;
	}
	else
	{
	    p->completed = 1 ;
	    if ( WIFSIGNALED(status) )
	    {
		fprintf(stderr, "%d : Terminated by signal %s\n",pid, WTERMSIG(status));
	    }
	    return 0 ;
	}
	fprintf(stderr, "No child process with pid = %d.\n",pid);
	return -1 ;
    }
    else if ( pid == 0 || errno ==  ECHILD )
	return -1 ;
    else
	perror("waitpid ");
    return -1 ;
}

void sig_chld(int n, siginfo_t *info , void *foo)
{
    printf("sig chld received from %d to %d\n",info->si_pid, getpid());
    process *p = find_process(info->si_pid) ;
    if ( p == NULL )
	printf("process not found!\n");
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

void print_bg_jobs(job *j)
{
    job *temp = j ;
    while ( temp != NULL )
    {
	if ( job_is_stopped(temp) && !job_is_completed(temp) )
	    printf("%s\n",temp->command);
	temp = temp->next_job ;
    }
}

void sig_cont(int n, siginfo_t *info, void *foo)
{
    printf("sig cont received from %d to %d\n",info->si_pid, getpid());
    process *p = find_process(getpid());
    if ( p == NULL )
	printf("process not found!\n");
    else
    {
	p->stopped = p->completed = 0 ;
    }
}

void print_process(process *p)
{

    printf("pid = %d, completed = %d , stopped = %d\n",p->pid, p->completed, p->stopped);
}

void print_job(job *j)
{
    process *temp = j->first_process ;
    while ( temp != NULL )
    {
	print_process(temp);
	temp = temp->next_process ;
    }
}
