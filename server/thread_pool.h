#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "queue.h"
#include <pthread.h>

typedef struct ThreadPool
{
    Queue* m_threads;
    Queue* m_tasks;
    pthread_mutex_t m_lock;
    pthread_cond_t m_task_ready;
    size_t m_num_threads;
    int m_end;
} ThreadPool;

int make_thread_pool(ThreadPool** thread_pool, size_t num_threads);
int destroy_thread_pool(ThreadPool* thread_pool);
int dispatch(ThreadPool* thread_pool, void (*task)(void*), void* arg);

#endif // THREAD_POOL_H
