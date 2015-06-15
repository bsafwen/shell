#include "prompt.h"

void prompt(char cwd[256],char Time[10], time_t *rawTime, struct tm **timeInfo)
{
    getcwd(cwd, 256);
    time(rawTime);
    *timeInfo = localtime(rawTime);
    strftime(Time, 10, "%Hh%M",*timeInfo);
    printf("il est %s.'%s' $ ",Time,cwd);
}
