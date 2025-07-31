#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

typedef struct QueueNode {
    struct QueueNode* m_next;
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
size_t queue_size(Queue* queue);

#endif // QUEUE_H
