// QueueA.h
#pragma once

#include <queue>
#include <mutex>

template<typename NodeType>
class QueueA {
private:
    std::queue<NodeType*> q;
    std::mutex mtx;

public:
    void enqueue(NodeType* node) {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(node);
    }

    NodeType* dequeue() {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty()) return nullptr;
        NodeType* node = q.front();
        q.pop();
        return node;
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