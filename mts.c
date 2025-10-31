#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#define NS_PER_SEC 1000000000

pthread_cond_t loading_done, crossing_done, timer_start; 
pthread_mutex_t queue_mutex, standby_mutex, crossing_mutex, crossed_mutex;  // In order: eastbound/westbound queues, standby, crossing, crossed counter

typedef struct train{
    int uid;                    // train id (starting at 0)
    int loading_time;           // how long until train is loaded from start
    int crossing_time;          // how long crossing will take
    int is_westbound;           // true if westbound, false if eastbound                  
    int is_high_priority;       // priority tracking
    struct train* previous;     // pointer to previous train in this train's queue
    struct timespec* start_time;// pointer to struct storing the start time for the program
    pthread_t* thread;          // pointer to thread representing this train
} train_info;

// roots of train queues
train_info* head_westbound;
train_info* head_eastbound;

int standby_count;          // shared variable counting number of trains being loaded into queues
int last_direction;         // shared variable counting number of trains waiting to be loaded into queues
int consecutive_count;      // shared variable counting number of trains that went the same direction
int crossing_count;         // counter for number of trains waiting to cross
int crossed_count;          // number of trains that have crossed

// Insert to queue where the given train_info pointer is the head
// new is the train to be inserted
// next_direction is the east/west direction of the previous train
// is_consecutive tracks if the last two trains went the same direction
train_info* insert_into_queue(train_info* head, train_info* new)
{   
    // Replace head if true
    // Conditions in order: priority checking, loading times, order in file
    if(head == NULL || new->is_high_priority > head->is_high_priority || new->loading_time < head->loading_time ||
        (new->loading_time == new->loading_time && new->uid < head->uid)) 
    {
        new->previous = head;
        return new;
    }// if
    return insert_into_queue(head->previous, new);
}// insert_into_queue

// Train thread method
void *train(void* train_void)
{
    train_info* train = (train_info*)train_void;

    // Hold until all threads are ready
    // Arbitrarily lock crossing and release it immediately
    while(train->start_time->tv_sec == -1) {
        pthread_cond_wait(&timer_start, &crossing_mutex);
    }// while
    pthread_mutex_unlock(&crossing_mutex);
    
    
    printf("loading start for train %d!\n", train->uid);

    usleep(train->loading_time*100000);

    printf("loading done for train %d!\n", train->uid);    

    // Insert train into queue
    pthread_mutex_lock(&standby_mutex);
    while(standby_count)
    {
        pthread_cond_wait(&loading_done, &queue_mutex);
    }// while
    standby_count++;
    pthread_mutex_unlock(&standby_mutex);

    printf("Train %d being inserted\n", train->uid);

    if(train->is_westbound)
    {
        head_westbound = insert_into_queue(head_westbound, train);
    } else 
    {
        head_eastbound = insert_into_queue(head_eastbound, train);
    }

    pthread_mutex_lock(&standby_mutex);
    standby_count--;
    pthread_mutex_unlock(&standby_mutex);
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&loading_done);

    printf("Train %d done insertion with west %d and east %d\n", train->uid, head_westbound==NULL?-1:head_westbound->uid, 
        head_eastbound==NULL?-1:head_eastbound->uid);    

    // Check if this train can cross
    // Checks in order: only train, next train, auto proceed if other head is null, starvation checking, priority, alternation
    pthread_mutex_lock(&crossing_mutex);
    crossing_count++;
    while(crossing_count-1 && (((head_westbound != NULL && head_westbound->uid == train->uid) || 
            (head_eastbound != NULL && head_eastbound->uid == train->uid)) ||
            head_eastbound == NULL || head_westbound == NULL ||
            (consecutive_count < 2 && train->is_westbound == last_direction) &&
            ((train->is_westbound && train->is_high_priority >= head_eastbound->is_high_priority) || 
            (!train->is_westbound && train->is_high_priority >= head_westbound->is_high_priority)) &&
            head_westbound->is_high_priority == head_eastbound->is_high_priority && train->is_westbound == last_direction)
        )
    {
        pthread_mutex_unlock(&crossing_mutex);
        pthread_cond_wait(&crossing_done, &crossing_mutex);
        printf("checking for train %d with %d trains waiting\n", train->uid, crossing_count);
    }// while

    if(last_direction == train->is_westbound)
    {
        consecutive_count = 0;
    }// if
    consecutive_count++;
    last_direction = train->is_westbound;
    

    pthread_mutex_lock(&queue_mutex);
    if(train->is_westbound) 
    {
        head_westbound = train->previous;
    } else 
    {
        head_eastbound = train->previous;
    }
    pthread_mutex_unlock(&queue_mutex);
    
    // Cross
    printf("Train %d is crossing!\n", train->uid);
    usleep(train->crossing_time*100000);

    pthread_mutex_unlock(&crossing_mutex);

    printf("after train %d is done, west is %d, east is %d\n", train->uid, head_westbound==NULL?-1:head_westbound->uid, 
        head_eastbound==NULL?-1:head_eastbound->uid);

    pthread_mutex_lock(&crossed_mutex);
    crossed_count++;
    pthread_mutex_unlock(&crossed_mutex);

    crossing_count--;

    free(train->thread);
    free(train);
    pthread_cond_broadcast(&crossing_done);
    pthread_exit(0);

    
}// train

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        exit(1);
    }
    FILE* fptr = fopen(argv[1], "r");

    
    // printf("start_time initial value: %lds, %ldns\n", start_time.tv_sec, start_time.tv_nsec);

    // mutex and convar init
    pthread_cond_init(&loading_done, NULL);
    pthread_cond_init(&crossing_done, NULL);
    pthread_cond_init(&timer_start, NULL);

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&standby_mutex, NULL);
    pthread_mutex_init(&crossing_mutex, NULL);
    pthread_mutex_init(&crossed_mutex, NULL);

    // Global values for trains
    standby_count = 0;
    struct timespec start_time = {-1, -1};

    // Load train template with default values
    train_info train_template = {-1, -1, -1, -1, -1, NULL, &start_time, (pthread_t*)pthread_self()};

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
        // new_train->previous = head;
        
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

        // Prepare for next train
        train_counter++;
    }// while

    // sleep(0.5);

    printf("loading started for all trains:\n");
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    pthread_cond_broadcast(&timer_start);
    
    // Check if there are any more trains each time one crosses
    pthread_mutex_lock(&crossed_mutex);
    while(crossed_count < train_counter)
    {
        printf("checking\n");
        pthread_mutex_unlock(&crossed_mutex);
        pthread_cond_wait(&crossing_done, &crossed_mutex);
        pthread_cond_signal(&crossing_done);
    }

    fclose(fptr);
}// main 