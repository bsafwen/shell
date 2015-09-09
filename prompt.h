#ifndef PROMPT_H
#define PROMPT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>

char prompt(char cwd[256],char Time[10], time_t *rawTime, struct tm **timeInfo);

#endif
