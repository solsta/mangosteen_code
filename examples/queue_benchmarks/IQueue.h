#pragma once

template<typename NodeType>
class IQueue {
public:
    virtual void enqueue(char *payload) = 0;
    virtual NodeType* dequeue() = 0;
    virtual ~IQueue() = default;
};