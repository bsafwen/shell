#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int ppid, cpid ;
    char buf[256];
    char *args[2];
    args[0] = "vim";
    args[1] = "NULL" ;
    ppid = getpid();

    if ( ! (cpid = fork()) )
    {
	setpgid(0, 0);
	tcsetpgrp(0, getpid());
	if (execvp("vim",args)< 0)
	    perror("execvp");
	exit(0);
    }
    if ( cpid < 0 )
    {
	perror("fork ");
	exit(EXIT_FAILURE);
    }
    setpgid(cpid, cpid);
    tcsetpgrp(0, ppid);

    waitpid(cpid, 0, 0);

    tcsetpgrp(0, ppid);

    while(1)
    {
	memset(buf, 0, 256);
	fgets(buf, 256, stdin);
	puts("ECHO:");
	puts(buf);
	puts("\n");
    }
    exit(EXIT_SUCCESS);
}
