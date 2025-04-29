#pragma once

template<typename NodeType>
class IQueue {
public:
    virtual void enqueue(NodeType* node) = 0;
    virtual NodeType* dequeue() = 0;
    virtual ~IQueue() = default;
};