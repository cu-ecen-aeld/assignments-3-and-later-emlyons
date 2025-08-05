#include "thread_pool_dynamic.h"
#include "error_handling.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct Task
{
    void (*task)(void*);
    void* arg;
    ThreadPool* thread_pool;
    QueueNode* self;
} Task;

void* _run_task(void* arg)
{
    Task* task = (Task*)arg;

    if (task == NULL)
    {
        fprintf(stderr, "task thread launched with null task\n");
        pthread_exit(NULL);
    }

    { // cleanup scope
        pthread_cleanup_push(free, task);
        task->task(task->arg);
        pthread_cleanup_pop(0);
    }

    pthread_mutex_lock(&task->thread_pool->m_lock);
    if (task->thread_pool->m_kill == 0)
    {
        if (task->self && task->self->m_data)
        {
            pthread_t* thread_id = (pthread_t*)task->self->m_data;
            if (queue_delete(task->thread_pool->m_threads, task->self))
            {
                pthread_mutex_unlock(&task->thread_pool->m_lock);
                free(task);
                fprintf(stderr, "task thread failed to delete from thread queue\n");
                pthread_exit(NULL);
            }
            if (queue_push_back(task->thread_pool->m_cleanup, thread_id) != 0)
            {
                pthread_mutex_unlock(&task->thread_pool->m_lock);
                free(task);
                fprintf(stderr, "task thread failed to add to cleanup queue\n");
                pthread_exit(NULL);
            }
        }
        else
        {
            pthread_mutex_unlock(&task->thread_pool->m_lock);
            free(task);
            fprintf(stderr, "task thread failed to cleanup\n");
            pthread_exit(NULL);
        }
    }
    pthread_mutex_unlock(&task->thread_pool->m_lock);
    free(task);
    return NULL;
}

int pool_make_thread_pool(ThreadPool** thread_pool)
{
    *thread_pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (*thread_pool == NULL)
    {
        RET_ERR("thread_pool failed to allocate");
    }

    if (pthread_mutex_init(&(*thread_pool)->m_lock, NULL) != 0)
    {
        RET_ERR("lock failed to init");
    }

    if (queue_make_queue(&(*thread_pool)->m_threads) != 0)
    {
        pthread_mutex_destroy(&(*thread_pool)->m_lock);
        free(*thread_pool);
        RET_ERR("thread queue failed to init");
    }

    if (queue_make_queue(&(*thread_pool)->m_cleanup) != 0)
    {
        pthread_mutex_destroy(&(*thread_pool)->m_lock);
        queue_destroy_queue((*thread_pool)->m_threads);
        free(*thread_pool);
        RET_ERR("cleanup queue failed to init");
    }
    
    (*thread_pool)->m_kill = 0;

    return 0;
}

int pool_destroy_thread_pool(ThreadPool* thread_pool)
{
    if (thread_pool == NULL)
    {
        RET_ERR("unexpected NULL");
    }

    pthread_mutex_lock(&thread_pool->m_lock);
    thread_pool->m_kill = 1; // with this set, threads will no longer access the queues
    pthread_mutex_unlock(&thread_pool->m_lock);

    if (thread_pool->m_threads != NULL)
    {
        pool_cleanup(thread_pool->m_threads);
        queue_destroy_queue(thread_pool->m_threads);
    }

    if (thread_pool->m_cleanup != NULL)
    {
        pool_cleanup(thread_pool->m_cleanup);
        queue_destroy_queue(thread_pool->m_cleanup);
    }

    pthread_mutex_destroy(&thread_pool->m_lock);
    free(thread_pool);
    return 0;
}

int pool_dispatch(ThreadPool* thread_pool, void (*task)(void*), void* arg)
{
    if (thread_pool == NULL)
    {
        RET_ERR("unexpected NULL");
    }

    Task* task_obj = (Task*)malloc(sizeof(Task));
    if (task_obj == NULL)
    {
        RET_ERR("failed to allocate task");
    }
    task_obj->task = task;
    task_obj->arg = arg;
    task_obj->thread_pool = thread_pool;

    pthread_t* thread_id = (pthread_t*)malloc(sizeof(pthread_t));
    if (thread_id == NULL)
    {
        free(task_obj);
        RET_ERR("thread_id failed to allocate");
    }

    pthread_mutex_lock(&thread_pool->m_lock);
    if (queue_push_back(thread_pool->m_threads, thread_id) != 0)
    {
        free(task_obj);
        free(thread_id);
        pthread_mutex_unlock(&thread_pool->m_lock);
        RET_ERR("thread queue failed to push");
    }
    task_obj->self = thread_pool->m_threads->m_tail;

    if (pthread_create(thread_id, NULL, _run_task, task_obj) != 0)
    {
        if (queue_delete(thread_pool->m_threads, task_obj->self) != 0)
        {
            RET_ERR("pthread failed and cleanup failed");
        }
        free(task_obj);
        free(thread_id);
        pthread_mutex_unlock(&thread_pool->m_lock);
        RET_ERR("pthread failed to create");
    }

    // clean up completed threads
    pool_cleanup(thread_pool->m_cleanup);
    
    pthread_mutex_unlock(&thread_pool->m_lock);
    return 0;
}

int pool_cleanup(Queue* queue)
{
    while (queue_size(queue) > 0)
    {
        pthread_t* thread_id;
        if (queue_front(queue, (void**)&thread_id) != 0)
        {
            RET_ERR("_cleanup() failed to access front()");
        }
        if (pthread_join(*thread_id, NULL) != 0)
        {
            free(thread_id);
            RET_ERR("_cleanup() failed to pthread_join()");
        }
        if (queue_pop(queue) != 0)
        {
            free(thread_id);
            RET_ERR("_cleanup() failed to pop()");
        }
        free(thread_id);
    }
    return 0;
}
