#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    thread_func_args->thread_complete_success = false;
    DEBUG_LOG("Sleep for %d mS before lock", thread_func_args->waitToObtainMs);
    usleep(thread_func_args->waitToObtainMs * 1000);
    


    pthread_mutex_lock(thread_func_args->mutex);
    DEBUG_LOG("LOCKED mutex");

    DEBUG_LOG("Sleep for %d mS before release", thread_func_args->waitToReleaseMs);

    usleep(thread_func_args->waitToReleaseMs * 1000);

    pthread_mutex_unlock(thread_func_args->mutex);
    DEBUG_LOG("UNLOCKED mutex");


    thread_func_args->thread_complete_success = true;

    
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
     struct thread_data *newThread = malloc(sizeof(struct thread_data*));

     if(newThread == NULL)
     {
        return false;
     }

     newThread->id = thread;
     newThread->waitToObtainMs = wait_to_obtain_ms;
     newThread->waitToReleaseMs = wait_to_release_ms;
     newThread->mutex = mutex;
     pthread_create(newThread->id, NULL, threadfunc, (void*)newThread);


    return newThread->thread_complete_success;
}

