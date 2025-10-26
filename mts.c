#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#define NS_PER_SEC 1000000000

pthread_cond_t loading_done, sending_done, timer_start;
pthread_mutex_t east_queue, west_queue, standby, crossing; 

typedef struct train{
    int uid;                    // train id (starting at 0)
    int loading_time;           // how long until train is loaded from start
    int crossing_time;          // how long crossing will take
    int is_westbound;           // true if westbound, false if eastbound                  
    int is_high_priority;       // priority tracking
    struct train* previous;     // pointer to previous train in this train's queue
    int* standby_count;         // pointer to shared variable counting number of trains waiting to be loaded into queues
    struct timespec* start_time;// pointer to struct storing the start time for the program
    pthread_t* thread;          // pointer to thread representing this train
} train_info;

// TEMPORARY: Only one linkedlist for data storage
train_info* root;

// Train thread method
void *train(void* train_void)
{
    train_info* train = (train_info*)train_void;
    printf("In thread for train %d\n", train->uid);
    // root = train->previous;
    // free(train->thread);

    // Arbitrarily lock crossing and release it immediately
    while(train->start_time->tv_sec == -1) {
        pthread_cond_wait(&timer_start, &crossing);
    }// while
    pthread_mutex_unlock(&crossing);
    
    // Hold until all threads are ready
    // 
    printf("loading start for train %d!\n", train->uid);

    usleep(train->loading_time*100000);

    printf("loading done for train %d!\n", train->uid);    

    return NULL;
}// train

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        exit(1);
    }
    FILE* fptr = fopen(argv[1], "r");

    int standby_count = 0;

    struct timespec start_time = {-1, -1};
    // printf("start_time initial value: %lds, %ldns\n", start_time.tv_sec, start_time.tv_nsec);

    // mutex and convar init
    pthread_cond_init(&loading_done, NULL);
    pthread_cond_init(&sending_done, NULL);
    pthread_cond_init(&timer_start, NULL);

    pthread_mutex_init(&west_queue, NULL);
    pthread_mutex_init(&east_queue, NULL);
    pthread_mutex_init(&standby, NULL);
    pthread_mutex_init(&crossing, NULL);

    // Load train template with default values for size allocation
    train_info train_template = {-1, -1, -1, -1, -1, NULL, &standby_count, &start_time, (pthread_t*)pthread_self()};

    // Train creation
    int train_counter = 0;
    int buffer[3];
    while(fscanf(fptr, "%c %d %d\n", (char*)&buffer[0], &buffer[1], &buffer[2]) == 3)
    {
        printf("Start of new loop!\n");

        // Copy new train struct
        train_info* new_train = malloc(sizeof(train_info));
        // train_info* new_train;
        memcpy(new_train, &train_template, sizeof(train_template));

        new_train->uid = train_counter;
        new_train->previous = root;
        
        // Read direction and priority
        switch((char)buffer[0])
        {
            case 'e':
                new_train->is_westbound = 0;
                new_train->is_high_priority = 0;
            case 'E':
                new_train->is_westbound = 0;
                new_train->is_high_priority = 1;
            case 'w':
                new_train->is_westbound = 1;
                new_train->is_high_priority = 0;
            case 'W':
                new_train->is_westbound = 1;
                new_train->is_high_priority = 1;
        }// switch

        // Read loading time
        new_train->loading_time = buffer[1];

        // Read crossing time
        new_train->crossing_time = buffer[2];

        // TESTING: print train
        printf("Train %d is going %s with %s priority. Loaded in %lfs and crosses in %lfs\n", new_train->uid, 
            new_train->is_westbound ? "west" : "east", new_train->is_high_priority ? "high" : "low", new_train->loading_time/10.0, new_train->crossing_time/10.0);

        // printf("thread created with code %d.\n", pthread_create(new_train->thread, NULL, train, (void*)new_train));
        pthread_t* new_thread = malloc(sizeof(pthread_t));
        pthread_create(new_thread, NULL, train, (void*)new_train);
        new_train->thread = new_thread;

        root = new_train;
        // Prepare for next train
        train_counter++;
    }// while

    // sleep(0.5);

    printf("loading started for all trains:\n");
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    pthread_cond_broadcast(&timer_start);
    
    // printf("value of start_time: %d\n", start_time->tv_sec);

    // sleep(15);

    // Change to wait for either west- or east-bound queue
    train_info* temp;
    while(root != NULL)
    {
        printf("joining train %d\n", root->uid);
        pthread_join(*root->thread, 0);
        temp = root;
        root = root->previous;
        // printf("is root null? %s\n", root == NULL ? "yes" : "no");
        free(temp->thread);
        free(temp);
    }// while

    fclose(fptr);
}// main 