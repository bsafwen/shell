#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define DELIMITERS " \t\r\n"

extern char *get_current_dir_name(void);

int main( int argc, char *argv[])
{
    char variable[1][100] ;
    char cmd[64];
    char bckp_cmd[64];
    char Time[10];
    char *word ;
    time_t rawTime ;
    struct tm *timeInfo ;
    int status ;
    unsigned vindex = 0 ;
    char cwd[256]; ;


    while ( 1 )
    {
        getcwd(cwd, 256);
        time(&rawTime);
        timeInfo = localtime(&rawTime);
        strftime(Time, 10, "%Hh%M",timeInfo);
        printf("il est %s.'%s' $ ",Time,cwd);
        fgets(cmd, 1024, stdin);
        if ( strcmp(cmd,"exit\n")== 0)
            break ;
        else if ( strcmp(cmd,"status\n") == 0 )
        {
            printf("%d\n",status);
            status = 0 ;
        }
        /* else if ( strcmp(cmd,"ls /\n")==0) */
            /* system(cmd); */
        else
        {
            strcpy(bckp_cmd,cmd);
            word = strtok(cmd,DELIMITERS);
            if( strcmp(word, "cd") == 0 )
            {
                word = strtok(NULL, DELIMITERS);
                if( chdir(word) < 0 )
                {
                    perror("");
                    status = 1 ;
                }
                else
                    status = 0 ;
            }
            else if ( strcmp(word,"set") == 0 )
            {
            }
            else
            {
                if ( system(bckp_cmd) < 0 )
                    perror("");
            }

        }
    }
    return 0 ;
}
