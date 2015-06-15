#include "apue.h"

int main(int argc, char *argv[] )
{
    if ( argc != 3 )
        err_quit("usage : cloneFile f1 f2");
    FILE *input, *output ;
    
    input = fopen(argv[1],"r");
    output = fopen(argv[2], "w");

    char c ;

    while ( (c = getc(input)) != EOF )
        putc(c, output);

    exit(0);
}
