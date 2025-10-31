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
    pthread_t* thread;          // pointer to thread representing this train
} train_info;

// roots of train queues
train_info* head_westbound;
train_info* head_eastbound;

int* standby_count;             // shared variable counting number of trains being loaded into queues
int* last_direction;            // shared variable counting number of trains waiting to be loaded into queues
int* consecutive_count;         // shared variable counting number of trains that went the same direction
int* crossing_count;            // counter for number of trains waiting to cross
int* crossed_count;             // number of trains that have crossed
train_info* join_buffer;        // buffer for joining of trains
FILE* fptrW;                    // pointer to output file
struct timespec* start_time;    // pointer to struct storing the start time for the program

// Get current simulation time
void get_time(char* buffer)
{
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    int hours = (current_time.tv_sec/(60*60)-start_time->tv_sec/(60*60))%100;
    int mins = (current_time.tv_sec/60-start_time->tv_sec/60)%60;
    int secs = (current_time.tv_sec-start_time->tv_sec)%60;
    int dsecs = (current_time.tv_nsec/10000000-start_time->tv_nsec/100000000)%100;
    snprintf(buffer, sizeof(char)*11, "%02d:%02d:%02d.%01d", hours, mins, secs, dsecs);
}// get_time()

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
    while(start_time->tv_sec == -1) {
        pthread_cond_wait(&timer_start, &crossing_mutex);
    }// while
    pthread_mutex_unlock(&crossing_mutex);

    usleep(train->loading_time*100000); 

    // Insert train into queue
    pthread_mutex_lock(&standby_mutex);
    while(*standby_count)
    {
        pthread_cond_wait(&loading_done, &queue_mutex);
    }// while
    (*standby_count)++;
    pthread_mutex_unlock(&standby_mutex);

    if(train->is_westbound)
    {
        head_westbound = insert_into_queue(head_westbound, train);
    } else 
    {
        head_eastbound = insert_into_queue(head_eastbound, train);
    }

    pthread_mutex_lock(&standby_mutex);
    (*standby_count)--;
    pthread_mutex_unlock(&standby_mutex);
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&loading_done);

    char time_buffer[11];
    get_time(time_buffer);
    fprintf(fptrW, "%11s Train %2d is ready to go %4s\n", time_buffer, train->uid, 
            train->is_westbound ? "West" : "East");

    // Check if this train can cross
    // Checks in order: only train, next train, auto proceed if other head is null, starvation checking, priority, alternation
    pthread_mutex_lock(&crossing_mutex);
    (*crossing_count)++;
    while((*crossing_count)-1 && (((head_westbound != NULL && head_westbound->uid == train->uid) || 
            (head_eastbound != NULL && head_eastbound->uid == train->uid)) &&
            (head_eastbound == NULL || head_westbound == NULL ||
            (*consecutive_count < 2 && train->is_westbound == *last_direction) &&
            ((train->is_westbound && train->is_high_priority >= head_eastbound->is_high_priority) || 
            (!train->is_westbound && train->is_high_priority >= head_westbound->is_high_priority)) &&
            head_westbound->is_high_priority == head_eastbound->is_high_priority && train->is_westbound == *last_direction))
        )
    {
        pthread_mutex_unlock(&crossing_mutex);
        pthread_cond_wait(&crossing_done, &crossing_mutex);
    }// while

    // Update fields related to last train that crossed
    if(*last_direction == train->is_westbound)
    {
        *consecutive_count = 0;
    }// if
    (*consecutive_count)++;
    *last_direction = train->is_westbound;
    
    // Update relevant queue
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

    get_time(time_buffer);
    fprintf(fptrW, "%11s Train %2d is ON the main track going %4s\n", time_buffer, train->uid, 
            train->is_westbound ? "West" : "East");

    usleep(train->crossing_time*100000);

    fprintf(fptrW, "%11s Train %2d is OFF the main track after going %4s\n", time_buffer, train->uid, 
            train->is_westbound ? "West" : "East");

    pthread_mutex_unlock(&crossing_mutex);

    // Update count of trains that have crossed and load buffer to join this train to main
    pthread_mutex_lock(&crossed_mutex);
    (*crossed_count)++;
    join_buffer = train;
    // Update count of actively crossing trains
    pthread_mutex_unlock(&crossed_mutex);
    (*crossing_count)--;
    pthread_cond_broadcast(&crossing_done);
    // Exit thread
    pthread_exit(0);    
}// train

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("Usage: %s filename\n", argv[0]);
        exit(1);
    }
    FILE* fptrR = fopen(argv[1], "r");
    fptrW = fopen("output.txt", "w");

    
    // printf("start_time initial value: %lds, %ldns\n", start_time.tv_sec, start_time.tv_nsec);

    // mutex and convar init
    pthread_cond_init(&loading_done, NULL);
    pthread_cond_init(&crossing_done, NULL);
    pthread_cond_init(&timer_start, NULL);

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&standby_mutex, NULL);
    pthread_mutex_init(&crossing_mutex, NULL);
    pthread_mutex_init(&crossed_mutex, NULL);


    // Global value init for trains
    int standby_default = 0;
    standby_count = &standby_default;
    int last_direction_default = 0;
    last_direction = &last_direction_default;
    int consecutive_default = 0;
    consecutive_count = &consecutive_default;
    int crossing_default = 0;
    crossing_count = &crossing_default;
    int crossed_default = 0;
    crossed_count = &crossed_default;
    struct timespec start_default = {-1, -1};
    start_time = &start_default;


    // Load train template with default values
    train_info train_template = {-1, -1, -1, -1, -1, NULL, (pthread_t*)pthread_self()};

    // Train creation
    int train_counter = 0;
    int buffer[3];
    while(fscanf(fptrR, "%c %d %d\n", (char*)&buffer[0], &buffer[1], &buffer[2]) == 3)
    {
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
                break;
            case 'E':
                new_train->is_westbound = 0;
                new_train->is_high_priority = 1;
                break;
            case 'w':
                new_train->is_westbound = 1;
                new_train->is_high_priority = 0;
                break;
            case 'W':
                new_train->is_westbound = 1;
                new_train->is_high_priority = 1;
                break;
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

    clock_gettime(CLOCK_MONOTONIC, start_time);
    pthread_cond_broadcast(&timer_start);
    
    // Check if there are any more trains each time one crosses
    pthread_mutex_lock(&crossed_mutex);
    while(*crossed_count < train_counter)
    {
        pthread_mutex_unlock(&crossed_mutex);
        pthread_cond_wait(&crossing_done, &crossed_mutex);
        pthread_join(*join_buffer->thread, 0);
        free(join_buffer->thread);
        free(join_buffer);
    }// while

    fclose(fptrR);
    fclose(fptrW);
}// main 