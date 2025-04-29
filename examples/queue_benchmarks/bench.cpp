#include <iostream>
#include <string>
#include <memory>
#include <pthread.h>

// ---------------------- Node ----------------------
template <size_t PayloadSize>
struct Node {
    char payload[PayloadSize];
    Node* next = nullptr;
};

// ---------------------- IQueue (templated interface) ----------------------
template <typename NodeType>
class IQueue {
public:
    virtual void enqueue(const char* payload) = 0;
    virtual NodeType* dequeue() = 0;
    virtual ~IQueue() = default;
};

// ---------------------- AbstractQueue (non-templated) ----------------------
class AbstractQueue {
public:
    virtual void enqueue(const char* payload) = 0;
    virtual void* dequeue() = 0;
    virtual ~AbstractQueue() = default;
};

// ---------------------- QueueA implementation ----------------------
template <typename NodeType>
class QueueA : public IQueue<NodeType> {
public:
    void enqueue(const char* payload) override {
        std::cout << "[QueueA] Enqueue: " << payload << "\n";
    }

    NodeType* dequeue() override {
        std::cout << "[QueueA] Dequeue\n";
        return nullptr;
    }
};

// ---------------------- QueueWrapper to bridge templates ----------------------
template <typename NodeType>
class QueueWrapper : public AbstractQueue {
    IQueue<NodeType>* impl;

public:
    explicit QueueWrapper(IQueue<NodeType>* q) : impl(q) {}

    void enqueue(const char* payload) override {
        impl->enqueue(payload);
    }

    void* dequeue() override {
        return static_cast<void*>(impl->dequeue());
    }

    ~QueueWrapper() override {
        delete impl;
    }
};

// ---------------------- Factory ----------------------
std::unique_ptr<AbstractQueue> make_queue(const std::string& type, size_t payload_size) {
    if (type == "QueueA" && payload_size == 64)
        return std::make_unique<QueueWrapper<Node<64>>>(new QueueA<Node<64>>());
    if (type == "QueueA" && payload_size == 128)
        return std::make_unique<QueueWrapper<Node<128>>>(new QueueA<Node<128>>());
    if (type == "QueueA" && payload_size == 256)
        return std::make_unique<QueueWrapper<Node<248>>>(new QueueA<Node<248>>());
    
    return nullptr;
}

// ---------------------- Thread Function ----------------------
void* thread_func(void* arg) {
    AbstractQueue* q = static_cast<AbstractQueue*>(arg);
    q->enqueue("Hello from thread");
    q->dequeue();
    return nullptr;
}

// ---------------------- Main ----------------------
int main(int argc, char** argv) {
    std::string queue_type = "QueueA";
    size_t payload_size = 128;

    if (argc >= 3) {
        queue_type = argv[1];
        payload_size = std::stoul(argv[2]);
    }

    auto queue = make_queue(queue_type, payload_size);
    if (!queue) {
        std::cerr << "Invalid queue type or payload size\n";
        return 1;
    }

    AbstractQueue* q_ptr = queue.get();

    pthread_t thread;
    if (pthread_create(&thread, nullptr, thread_func, q_ptr) != 0) {
        std::cerr << "Failed to create thread\n";
        return 1;
    }

    pthread_join(thread, nullptr);
    return 0;
}
