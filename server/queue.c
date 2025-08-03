#include "queue.h"
#include "error_handling.h"
#include <stdlib.h>
#include <stdio.h>

static int _make_node(void* data, QueueNode** node);

int queue_make_queue(Queue** queue)
{
    *queue = (Queue*)malloc(sizeof(Queue));
    if (*queue == NULL)
    {
        RET_ERR("unexpected NULL");
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
        RET_ERR("unexpected NULL");
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
    if (queue == NULL)
    {
        RET_ERR("unexpected NULL");
    }

    if (queue_size(queue) == 0)
    {
        RET_ERR("pop() failed. empty queue");
    }
    
    if (queue->m_head == queue->m_tail)
    {
        free(queue->m_head);
        queue->m_head = NULL;
        queue->m_tail = NULL;
    }
    else {
        QueueNode* next = queue->m_head->m_next;
        next->m_last = NULL;
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
        RET_ERR("unexpected NULL");
    }

    QueueNode* node;
    if (_make_node(data, &node) == -1)
    {
        RET_ERR("push() failed. failed to create node");
    }
    else if (queue->m_head == NULL && queue->m_tail == NULL)
    {
        queue->m_head = node;
        queue->m_tail = node;
    }
    else if (queue->m_tail != NULL)
    {
        node->m_last = queue->m_tail;
        queue->m_tail->m_next = node;
        queue->m_tail = node;
    }
    else
    {
        RET_ERR("push() failed. invalid state");
    }
    queue->m_size++;
    return 0;
}

int queue_front(Queue* queue, void** data)
{
    if (queue == NULL)
    {
        RET_ERR("unexpected NULL");
    }
    if (queue_size(queue) == 0)
    {
        RET_ERR("front() failed. empty queue");
    }
    *data = queue->m_head->m_data;
    return 0;
}

int queue_delete(Queue* queue, QueueNode* node)
{
    if (queue == NULL)
    {
        RET_ERR("unexpected NULL");
    }
    if (node == NULL)
    {
        RET_ERR("delete() failed. invalid node");
    }
    if (queue_size(queue) == 0)
    {
        RET_ERR("delete() failed. empty queue");
    }

    if (node->m_last && node->m_next) // middle node
    {
        node->m_last->m_next = node->m_next;
        node->m_next->m_last = node->m_last;
    }
    else if (node->m_last) // tail node
    {
        node->m_last->m_next = NULL;
        queue->m_tail = node->m_last;
    }
    else if (node->m_next) // head node
    {
        node->m_next->m_last = NULL;
        queue->m_head = node->m_next;
    }
    else // only node
    {
        queue->m_head = NULL;
        queue->m_tail = NULL;
    }
    free(node);
    queue->m_size--;
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

static int _make_node(void* data, QueueNode** node)
{  
    *node = (QueueNode*)malloc(sizeof(QueueNode));
    if (*node == NULL)
    {
        RET_ERR("unexpected NULL");
    }
    (*node)->m_next = NULL;
    (*node)->m_last = NULL;
    (*node)->m_data = data;
    return 0;
}
