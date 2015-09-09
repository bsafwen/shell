#include "prompt.h"
extern char *lookup(char *name);
#define DELIM "@%"

char prompt(char cwd[256],char Time[10], time_t *rawTime, struct tm **timeInfo)
{
    int n ;
    char color_format[32]="\x1B[38;2;";
    char time_format[32]="il est ";
    char *result = lookup("PROMPT") ;
    char *endptr ;
    if ( result == NULL )
	printf("$ ");
    else
    {
	char *PROMPT = strdup(result);
	char display_directory = 0 ;
	char display_time = 0 ;
	char display_user = 0 ;
	char display_host = 0 ;
	char *word = strtok(PROMPT, DELIM);
	n = strtol(word, &endptr, 10);
	if ( endptr == word )
	{
	    printf("Invalid color code: R@G@B\n$ ");
	    return 0 ;
	}
	strcat(color_format,word);
	strcat(color_format,";");
	word = strtok(NULL, DELIM);
	n = strtol(word, &endptr, 10);
	if ( endptr == word )
	{
	    printf("Invalid color code: R@G@B\n$ ");
	    return 0 ;
	}
	strcat(color_format,word);
	strcat(color_format,";");
	word = strtok(NULL, DELIM);
	n = strtol(word, &endptr, 10);
	if ( endptr == word )
	{
	    printf("Invalid color code: R@G@B\n$ ");
	    return 0 ;
	}
	strcat(color_format,word);
	strcat(color_format,"m");
	printf(color_format);
	while ( (word = strtok(NULL, DELIM)) != NULL )
	{
	    if ( strcmp(word, "U") == 0 )
	    {
		display_user = 1 ;
	    }
	    else if ( strcmp(word, "HOST") == 0 )
	    {
		display_host = 1 ;
	    }
	    else if ( strcmp(word, "P") == 0 )
	    {
		display_directory = 1 ;
		getcwd(cwd,256);
	    }
	    else if ( strcmp(word, "H") == 0 )
	    {
		display_time = 1 ;
		strcat(time_format,"%Hh");
	    }
	    else if ( strcmp(word, "M") == 0 )
	    {
		strcat(time_format,"%Mm");
		display_time = 1 ;
	    }
	    else if ( strcmp(word, "S") == 0 )
	    {
		strcat(time_format,"%Ss");
		display_time = 1 ;
	    }
	}
	if ( display_user == 1 )
	{
	    printf("%s:",getlogin());
	}
	if ( display_host == 1 )
	{
	    char host[16] ;
	    gethostname(host, 16);
	    printf("@%s",host);
	}
	if ( display_time == 1 )
	{
	    time(rawTime);
	    *timeInfo = localtime(rawTime);
	    strftime(Time, 32, time_format,*timeInfo);
	    printf(Time);
	}
	if ( display_directory == 1 )
	    printf(".%s",cwd);
	printf("$ ");
	free(PROMPT);
    }
    printf("\x1B[0m");
}
