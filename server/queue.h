#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <pthread.h>

typedef struct QueueNode {
    struct QueueNode* m_next;
    struct QueueNode* m_last;
    void* m_data;
} QueueNode;

typedef struct Queue {
    QueueNode* m_head;
    QueueNode* m_tail;
    int m_size;
} Queue;

int queue_make_queue(Queue** queue);
int queue_destroy_queue(Queue* queue);
int queue_pop(Queue* queue);
int queue_push_back(Queue* queue, void* data);
int queue_front(Queue* queue, void** data);
int queue_delete(Queue* queue, QueueNode* node);
size_t queue_size(Queue* queue);

#endif // QUEUE_H
