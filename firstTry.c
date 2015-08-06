#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define DELIMITERS " ,)"

extern char **environ ;

int main( int argc, char *argv[])
{
    int fd = open("/dev/tty",O_RDWR);
    tcsetpgrp(fd, getpid());
    char **envptr = environ  ;
    pid_t pid ;
    char variable[1][100] ;
    char cmd[64];
    char bckp_cmd[64];
    char Time[10];
    char *word, *ptr ;
    time_t rawTime ;
    struct tm *timeInfo ;
    int status = 0 ;
    unsigned vindex = 0 ;
    char cwd[256]; ;


    printf("%llu   -   %llu\n",(long long unsigned)getpid(),(long long unsigned)getpgid(getpid()));
    while ( 1 )
    {
	prompt(cwd,Time, &rawTime, &timeInfo);
	fgets(cmd, 1024, stdin);
	if( cmd[0] != '\n' )
	{
	    if ( strcmp(cmd,"exit\n")== 0)
		break ;
	    else if ( strcmp(cmd,"status\n") == 0 )
	    {
		printf("%d\n",status);
		status = 0 ;
	    }
	    else if ( strcmp(cmd,"show\n") == 0 )
	    {
		while ( *envptr != NULL )
		{
		    printf("%s\n",*envptr);
		    ++envptr ;
		}
		envptr = environ ;
		status = 0 ;
	    }
	    else {
		if ( cmd[strlen(cmd)-1]=='\n' )
		    cmd[strlen(cmd)-1]='\0' ;
		strcpy(bckp_cmd,cmd);
		word = strtok(bckp_cmd, DELIMITERS);
		if ( strcmp(word, "cd") == 0 )
		{
		    word = strtok(NULL, DELIMITERS);
		    if (   chdir(word) == -1  )
		    {
			perror("chdir ");
			status = 1 ;
			continue ;
		    }
		}
		else if ( strcmp(word, "set") == 0 )
		{
		    word = strtok(NULL, DELIMITERS);
		    ptr = strtok(NULL,DELIMITERS);
		    setenv(word, ptr, 1);
		    status = 1 ;
		    continue ;
		}
		else if ( strcmp(word, "unset") == 0 )
		{
		    word = strtok(NULL, DELIMITERS);
		    unsetenv(word);
		    status = 0 ;
		    continue ;
		}
		else {
		    if ( (pid=fork()) < 0 )
		    {
			perror("fork ");
			exit(EXIT_FAILURE);
		    }
		    else if ( pid == 0 )
		    {
			/* setsid(); */
			printf("%llu	-   %llu\n",(long long unsigned)getpid(),(long long unsigned)getpgid(getpid()));
			system(cmd);
			exit(EXIT_SUCCESS);
		    }
		    else
			wait(NULL);
		}
	    }
	}
    }
    return 0 ;
}
