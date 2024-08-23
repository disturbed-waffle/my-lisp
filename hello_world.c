#include <stdio.h>

void hello(int n, char* msg){
for(int i=0;i<n;i++){
    puts("Hello, world");
}    
puts(msg);
}

int main(){
    typedef struct hello_world
    {
    int i;
    }my_struct;
    my_struct joe;
    joe.i = 5;
    my_struct john;
    john.i = 15;

    printf("%d",joe.i + john.i);

    

    int n = 69;
    char* extra_message;
    switch (n)
    {
    case 69: 
        extra_message = "nice!";
        break;
    case 420:
        extra_message = "epic!";
        break;
    default:
        extra_message = "cringe";
        break;
    }
    hello(n,extra_message);
    return 0;
}