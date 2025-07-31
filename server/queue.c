#include "queue.h"
#include <stdlib.h>
#include <assert.h>

int make_node(void* data, QueueNode** node)
{  
    *node = (QueueNode*)malloc(sizeof(QueueNode));
    if (*node == NULL)
    {
        return -1;
    }
    (*node)->m_next = NULL;
    (*node)->m_data = data;
    return 0;
}

int queue_make_queue(Queue** queue)
{
    *queue = (Queue*)malloc(sizeof(Queue));
    if (*queue == NULL)
    {
        return -1;
    }
    (*queue)->m_head = NULL;
    (*queue)->m_tail = NULL;
    (*queue)->m_size = 0;
    return 0;
}

int queue_destroy_queue(Queue* queue)
{
    if (queue == NULL)
    {
        return -1;
    }

    while (queue->m_head != NULL)
    {
        queue_pop(queue);
    }
    free(queue);
    return 0;
}

int queue_pop(Queue* queue)
{
    if (queue == NULL || queue->m_head == NULL)
    {
        return -1;
    }
    
    assert(queue->m_size > 0);
    if (queue->m_head == queue->m_tail)
    {
        free(queue->m_head);
        queue->m_head = NULL;
        queue->m_tail = NULL;
    }
    else {
        QueueNode* next = queue->m_head->m_next;
        free(queue->m_head);
        queue->m_head = next;
    }
    
    queue->m_size--;
    return 0;
}

int queue_push_back(Queue* queue, void* data)
{
    if (queue == NULL)
    {
        return -1;
    }

    QueueNode* node;
    if (make_node(data, &node) == -1)
    {
        return -1;
    }
    else if (queue->m_head == NULL)
    {
        queue->m_head = node;
        queue->m_tail = node;
    }
    else 
    {
        queue->m_tail->m_next = node;
        queue->m_tail = node;
    }
    queue->m_size++;
    return 0;
}

int queue_front(Queue* queue, void** data)
{
    if (queue == NULL || queue->m_head == NULL)
    {
        return -1;
    }
    *data = queue->m_head->m_data;
    return 0;
}

size_t queue_size(Queue* queue)
{
    if (queue == NULL)
    {
        return 0;
    }
    return queue->m_size;
}
