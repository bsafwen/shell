#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#define DELIMITERS " ,)"

extern char **environ ;

int main( int argc, char *argv[])
{
    char **envptr = environ  ;
    int er ;
    pid_t pid ;
    char variable[1][100] ;
    char cmd[64];
    char bckp_cmd[64];
    char Time[10];
    char *word ;
    time_t rawTime ;
    struct tm *timeInfo ;
    int status = 0 ;
    unsigned vindex = 0 ;
    char cwd[256]; ;


    while ( 1 )
    {
        prompt(cwd,Time, &rawTime, &timeInfo);
        fgets(cmd, 1024, stdin);
        if ( strcmp(cmd,"exit\n")== 0)
            break ;
        else if ( strcmp(cmd,"status\n") == 0 )
        {
            printf("%d\n",status);
            status = 0 ;
            continue ;
        }
        else if ( strcmp(cmd,"show\n") == 0 )
        {
            while ( *envptr != NULL )
            {
                printf("%s\n",*envptr);
                ++envptr ;
            }
            status = 0 ;
        }
        else {
            if ( cmd[strlen(cmd)-1]=='\n' )
                cmd[strlen(cmd)-1]='\0' ;
            if ( (pid=fork()) < 0 )
            {
                perror("fork ");
                exit(EXIT_FAILURE);
            }
            else if ( pid == 0 )
            {
                 if ( (er = system(cmd)) != 0 )
                 {
                     if (er == -1 )
                         perror(cmd);
                     status = 1 ;
                 }
                 else 
                     status = 0 ;
                 exit(EXIT_SUCCESS);
            }
            else
                wait(NULL);
        }
        /* else if ( strcmp(cmd,"status\n") == 0 ) */
        /* { */
        /*     printf("%d\n",status); */
        /*     status = 0 ; */
        /* } */
        /* /1* else if ( strcmp(cmd,"ls /\n")==0) *1/ */
        /* /1* system(cmd); *1/ */
        /* else */
        /* { */
        /*     strcpy(bckp_cmd,cmd); */
        /*     word = strtok(cmd,DELIMITERS); */
        /*     if( strcmp(word, "cd") == 0 ) */
        /*     { */
        /*         word = strtok(NULL, DELIMITERS); */
        /*         if( chdir(word) < 0 ) */
        /*         { */
        /*             perror(""); */
        /*             status = 1 ; */
        /*         } */
        /*         else */
        /*             status = 0 ; */
        /*     } */
        /*     else if ( strcmp(word,"set") == 0 ) */
        /*     { */
        /*     } */
        /*     else */
        /*     { */
        /*         if ( system(bckp_cmd) < 0 ) */
        /*             perror(""); */
        /*     } */

        /* } */
    }
    return 0 ;
}
