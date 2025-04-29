// QueueAWrapper.h
#pragma once

#include "IQueue.h"
#include "QueueA.h"

template<typename NodeType>
class QueueAWrapper : public IQueue<NodeType> {
private:
    QueueA<NodeType> q;

public:
    void enqueue(char *payload) override {
        q.enqueue(payload);
    }

    NodeType* dequeue() override {
        return q.dequeue();
    }
};

/*
#pragma once

#include "QueueInterface.h"
#include "QueueLockBased.h"

template<typename NodeType>
class WrapperQueueLockBased : public QueueInterface<NodeType> {
private:
    QueueLockBased<NodeType> q;

public:

    QueueLockBased() = default;  
    
    void enqueue(const char *payload) override {
        q.enqueue(payload);
    }

    NodeType* dequeue() override {
        return q.dequeue();
    }
};
*/