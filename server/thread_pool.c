#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct Task
{
    void (*task)(void*);
    void* arg;
} Task;

void* task_poll(void* arg)
{
    ThreadPool* thread_pool = (ThreadPool*)arg;
    pthread_t thread_id = pthread_self();
    while (1)
    {
        pthread_mutex_lock(&thread_pool->m_lock);
        while (queue_size(thread_pool->m_tasks) == 0 && thread_pool->m_end == 0) {
            pthread_cond_wait(&thread_pool->m_task_ready, &thread_pool->m_lock);
        }

        if (thread_pool->m_end != 0) { // exit thread
           pthread_mutex_unlock(&thread_pool->m_lock);
           return NULL;
        }

        Task* task;
        if (queue_front(thread_pool->m_tasks, (void**)&task) == 0)
        {
            if (queue_pop(thread_pool->m_tasks)) 
            {
                fprintf(stderr, "failed to remove task after acquiring");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            task = NULL;
            fprintf(stderr, "failed to acquire task");
        }
        
        pthread_mutex_unlock(&thread_pool->m_lock);

        if (task != NULL)
        {
            task->task(task->arg);
            free(task);
        }
    }
}

int launch_thread(ThreadPool* thread_pool)
{
    pthread_t* thread_id = (pthread_t*)malloc(sizeof(pthread_t));
    if (thread_id == NULL)
    {
        return -1;
    }
    if (pthread_create(thread_id, NULL, task_poll, thread_pool) != 0)
    {
        free(thread_id);
        return -1;
    }
    if (queue_push_back(thread_pool->m_threads, thread_id) != 0)
    {
        pthread_cancel(*thread_id);
        pthread_join(*thread_id, NULL);
        free(thread_id);
        return -1;
    }
    return 0;
}

void destroy_thread_queue(ThreadPool* thread_pool, Queue* thread_queue)
{
    while (queue_size(thread_queue))
    {
        pthread_t* thread_id;
        if (queue_front(thread_queue, (void**)&thread_id) != 0)
        {
            fprintf(stderr, "critical error when destroying threads");
            exit(EXIT_FAILURE);
        }
        if (queue_pop(thread_queue) != 0)
        {
            fprintf(stderr, "critical error when destroying threads");
            exit(EXIT_FAILURE);
        }
        pthread_join(*thread_id, NULL);
        free(thread_id);
    }
    queue_destroy_queue(thread_queue);
}

int make_thread_pool(ThreadPool** thread_pool, size_t num_threads)
{
    *thread_pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (*thread_pool == NULL)
    {
        return -1;
    }
    if (queue_make_queue(&(*thread_pool)->m_threads) != 0)
    {
        return -1;
    }
    if (queue_make_queue(&(*thread_pool)->m_tasks) !=0)
    {
        return -1;
    }
    (*thread_pool)->m_end = 0;
    (*thread_pool)->m_num_threads = num_threads;

    pthread_cond_init(&(*thread_pool)->m_task_ready, NULL);
    pthread_mutex_init(&(*thread_pool)->m_lock, NULL);

    for (int i = 0; i < (*thread_pool)->m_num_threads; i++)
    {
        if (launch_thread(*thread_pool) != 0)
        {
            fprintf(stderr, "critical error launching threads");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

int destroy_thread_pool(ThreadPool* thread_pool)
{
    if (thread_pool == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&thread_pool->m_lock);
    thread_pool->m_end = 1;
    pthread_cond_broadcast(&thread_pool->m_task_ready);
    pthread_mutex_unlock(&thread_pool->m_lock);

    destroy_thread_queue(thread_pool, thread_pool->m_threads);
    queue_destroy_queue(thread_pool->m_tasks);

    pthread_mutex_destroy(&thread_pool->m_lock);
    pthread_cond_destroy(&thread_pool->m_task_ready);

    free(thread_pool);

    return 0;
}

int dispatch(ThreadPool* thread_pool, void (*task)(void*), void* arg)
{
    if (thread_pool == NULL)
    {
        return -1;
    }

    Task* new_task = malloc(sizeof(Task));
    if (!new_task)
    {
        return -1;
    }

    new_task->task = task;
    new_task->arg = arg;

    pthread_mutex_lock(&thread_pool->m_lock);
    if (queue_push_back(thread_pool->m_tasks, new_task) != 0)
    {
        free(new_task);
        pthread_mutex_unlock(&thread_pool->m_lock);
        return -1;
    }
    pthread_cond_signal(&thread_pool->m_task_ready);
    pthread_mutex_unlock(&thread_pool->m_lock);

    return 0;
}
