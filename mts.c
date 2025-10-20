#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_cond_t loading_done, sending_done, timer_start;
pthread_mutex_t east_queue, west_queue, standby, crossing; 

struct train{
    int loading_time;
    int crossing_time;
    char direction;
    int is_high_priority;
    struct train* previous;
    int* standby_count;
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        exit(1);
    }
    FILE* fptr = fopen(argv[1], "r");
    char buffer[20];
    fscanf(fptr, "%s", buffer);
    printf("%s\n", buffer);
    fscanf(fptr, "%s", buffer);
    printf("%s\n", buffer);

    // Train creation
    while(buffer != NULL)
    {

        fscanf(fptr, buffer);
    }// while
}// main 