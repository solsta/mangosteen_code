// QueueA.h
#pragma once

#include "node.h"

template<typename NodeType>
class QueueA {

public:
NodeType* front;
NodeType* rear;

QueueA () {
    NodeType *node = (NodeType*)malloc(sizeof(NodeType));
    node->next = NULL;
    front = rear = node;
}


void enqueue(char *payload) {
    printf("Called enqueue\n");
    NodeType* newNode = (NodeType*)malloc(sizeof(NodeType));
    printf("Copying %d bytes\n",sizeof(NodeType));
    printf("Src: %s\n", payload);
    memcpy(newNode->payload, (void*)payload, sizeof(NodeType));
    printf("Node payload: %s\n", newNode->payload);
    newNode->next = NULL;
    rear->next = newNode;
    rear = newNode;
}

NodeType* dequeue() {
    NodeType* node = front;
    NodeType *new_head = node->next;
    if (new_head == NULL) {
        return NULL;
    }
    printf("Node payload: %s\n", new_head->payload);
    front = new_head;
    free(node);
    return NULL;
}
};

/*
template<typename NodeType>
class QueueLockBased {

public:
    NodeType* front;
    NodeType* rear;
    void enqueue(const char* str) {
        NodeType* newNode = (NodeType*)malloc(sizeof(NodeType));
        memcpy(newNode->data, (void*)str, sizeof(NodeType));
        newNode->next = NULL;
        rear->next = newNode;
        rear = newNode;
    }

    NodeType* dequeue() {
        NodeType* node = front;
        NodeType *new_head = node->next;
        if (new_head == NULL) {
            return NULL;
        }
        printf("%s", node.payload);
        front = new_head;
        free(node);
        return NULL;
    }

    QueueLockBased (size_t payload_size) {
        NodeType *node = (NodeType*)malloc(sizeof(NodeType));
        node->next = NULL;
        front = rear = node;
    }
    ~QueueLockBased(){
        NodeType* current = front;
        while (current != NULL) {
            NodeType* next = current->next;
            free(current);
            current = next;
        }
    }
};

*/