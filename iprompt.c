#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

int main(int argc, char** argv){
// Version and exit info
puts("JanLisp Version 0.01");
puts("Cntrl+c to Exit!\n>>>");

while (1)
{
    // prompt and get inpu
    char* input = readline(">>> ");

    // store history
    add_history(input);

    // echo input
    printf("No, why don't you %s\n", input);

    // Free retrieved input
    free(input);
}   


return 0;

}