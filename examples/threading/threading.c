#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // wait, obtain mutex, wait, release mutex as described by thread_data structure
    if (thread_param == NULL)
    {
        fprintf(stderr, "thread_param is NULL");
        return thread_param;
    }

    thread_data* p_data = (thread_data*)thread_param;
    p_data->thread_complete_success = false;
    

    if (usleep(p_data->wait_to_obtain_ms*1000) != 0)
    {
        perror("thread failed to wait to obtain");
        return thread_param;
    }
    
    if (pthread_mutex_lock(p_data->mutex) != 0)
    {
        perror("failed to obtain mutex");
        return thread_param;
    }

    bool status = true;
    if (usleep(p_data->wait_to_release_ms*1000) != 0)
    {
        pthread_mutex_unlock(p_data->mutex);
        status = false;
        perror("thread failed to wait to release");
    }

    if (pthread_mutex_unlock(p_data->mutex) != 0)
    {
        status = false;
        perror("failed to release mutex");
    }
    
    p_data->thread_complete_success = status;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    thread_data* p_thread_data = calloc(1, sizeof(thread_data));
    p_thread_data->mutex = mutex;
    p_thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    p_thread_data->wait_to_release_ms = wait_to_release_ms;
    
    if (pthread_create(thread, NULL, threadfunc, p_thread_data) != 0)
    {
        perror("failed to launch thread");
        return false;
    }
    return true;
}

