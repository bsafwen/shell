#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* extern char *get_current_dir_name(void); */

int main()
{
    char w[256] ;
    getcwd(w,256);
    printf("%s\n",w);
    return 0 ;
}
