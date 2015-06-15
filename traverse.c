#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int traverse(char *, struct stat * infostatp);

int main(int argc, char *argv[])
{
    struct stat infostat ;
    if ( argc != 2 )
    {
        printf("Usage : travers dirName\n");
    }
    traverse(argv[1], &infostat);
    return 0 ;
}


int traverse(char *dirname, struct stat * infostatp)
{
    stat(dirname, infostatp);
    if ( ! S_ISDIR(infostatp->st_mode) )
    {
        printf("%s\n",dirname);
        return 0 ;
    }
    else
    {
        printf("%s",dirname);
        if (dirname[strlen(dirname)-1]!='/')
            printf("/\n");
        else
            printf("\n");
        char path[256] ;
        DIR *dir ;
        struct dirent *direp ;
        if ( (dir = opendir(dirname)) == NULL )
        {
            perror("opendir ");
            return 0 ;
        }
        while ( (direp = readdir(dir)) != NULL )
        {
            if ( (strcmp(direp->d_name,".") != 0) && (strcmp(direp->d_name,"..")!=0) ) 
            {
                strcpy(path,dirname);
                if (path[strlen(path)-1]!='/')
                    strcat(path,"/");
                strcat(path, direp->d_name);
                traverse(path, infostatp);
            }
        }
        closedir(dir);
    }
    return 0 ;
}
