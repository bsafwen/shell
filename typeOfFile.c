#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if ( argc != 2 )
    {
        printf("Usage : ./a.out fileName\n");
        exit(EXIT_FAILURE);
    }
    struct stat infos ;
    if ( stat(argv[1], &infos) )
    {
        perror("stat ");
        exit(EXIT_FAILURE);
    }
    printf("size = %lu\n",infos.st_size);
    printf("mode = %u\n",infos.st_mode);
    if ( S_ISREG(infos.st_mode)) 
        printf("%s is a regular file.\n",argv[1]);
    else if ( S_ISDIR(infos.st_mode) )
        printf("%s is a directory.\n",argv[1]);
    else 
        printf("%s is a link.\n",argv[1]);

    return 0 ;
}
