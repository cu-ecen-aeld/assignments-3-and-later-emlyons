#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "queue.h"
#include <pthread.h>

typedef struct ThreadPool
{
    Queue* m_threads;
    Queue* m_cleanup;
    pthread_mutex_t m_lock;
    int m_kill;
} ThreadPool;

int pool_make_thread_pool(ThreadPool** thread_pool);
int pool_destroy_thread_pool(ThreadPool* thread_pool);
int pool_dispatch(ThreadPool* thread_pool, void (*task)(void*), void* arg);
int pool_cleanup(Queue* queue);

#endif // THREAD_POOL_H
