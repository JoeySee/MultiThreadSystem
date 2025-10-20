#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_cond_t timer_start, 

struct train{

}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        exit(1);
    }
    FILE* fptr = fopen(argv[1], "r");
    char buffer[100];
    fscanf(fptr, "%s", buffer);
    printf("%s\n", buffer);
    fscanf(fptr, "%s", buffer);
    printf("%s\n", buffer);

    // Train creation
}// main 