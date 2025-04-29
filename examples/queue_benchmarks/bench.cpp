#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"
#include "/home/se00598/home/se00598/vanila_flit/flit/common/rand_r_32.h"
#include <jemalloc/jemalloc.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <iostream>
#include <string>
#include <memory>
#include <string.h>

// ---------------------- Mongosteen code ----------------------
thread_local serialized_app_command serializedAppCommand;
#define SM_OP_ENQUEUE 0
#define SM_OP_DEQUEUE 1

constexpr uint64_t POINTER_MASK = 0x0000FFFFFFFFFFFF;  // Lower 48 bits
constexpr uint64_t COUNTER_MASK = 0xFFFF000000000000;  // Upper 16 bits

// ---------------------- Node ----------------------
template <size_t PayloadSize>
struct Node {
    char payload[PayloadSize];
    Node* next = nullptr;
};

template <size_t PayloadSize>
struct LockFreeNode {
    char payload[PayloadSize];
    LockFreeNode* next = nullptr;
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

NodeType* front;
NodeType* rear;

QueueA () {
    NodeType *node = (NodeType*)malloc(sizeof(NodeType));
    node->next = NULL;
    front = rear = node;
}
    void enqueue(const char* payload) override {
        NodeType* newNode = (NodeType*)malloc(sizeof(NodeType));
        memcpy(newNode->payload, (void*)payload, sizeof(NodeType));
        newNode->next = NULL;
        rear->next = newNode;
        rear = newNode;
    }

    NodeType* dequeue() override {
        NodeType* node = front;
        NodeType *new_head = node->next;
        if (new_head == NULL) {
            return NULL;
        }
        front = new_head;
        free(node);
        return nullptr;
    }
};

// ---------------------- QueueB implementation ----------------------
template <typename NodeType>
class QueueB : public IQueue<NodeType> {
public:
    NodeType* front;
    NodeType* rear;
    
    QueueB () {
        front = rear = NULL;
    }

    void enqueue(const char* payload) override {
        NodeType* newNode = (NodeType*)malloc(sizeof(NodeType));
        memcpy(newNode->payload, (void*)payload, sizeof(NodeType));
        newNode->next = NULL;
    
        if (rear == NULL) {
            front = rear = newNode;
        } else {
            rear->next = newNode;
            rear = newNode;
        }
    }

    NodeType* dequeue() override {
        if (front == NULL) {
            return NULL;
        }
        NodeType* responceNode = front;
        front = front->next;
        if (front == NULL) {
            rear = NULL;
        }
        free(responceNode);
        return nullptr;
    }
};

// ---------------------- QueueC implementation ----------------------
template <typename NodeType>
class QueueC : public IQueue<NodeType> {
private:
uint64_t pack(NodeType* ptr, uint16_t counter) {
    uint64_t ptrBits = reinterpret_cast<uint64_t>(ptr) & POINTER_MASK;
    uint64_t countBits = static_cast<uint64_t>(counter) << 48;
    return ptrBits | countBits;

}

NodeType* unpack_ptr(uint64_t packed) {
    return reinterpret_cast<NodeType*>(packed & POINTER_MASK);

}

uint16_t unpack_counter(uint64_t packed) {
    return static_cast<uint16_t>(packed >> 48);

}

public:

std::atomic<uint64_t> front; // This is actually a packed Pointer
std::atomic<uint64_t> rear;

QueueC(){
    NodeType *node = (NodeType*)malloc(sizeof(NodeType));
    uint64_t nodePtr = pack(node,0);
    front = rear = nodePtr;
}
    void enqueue(const char* payload) override {

        NodeType *node = (NodeType*)malloc(sizeof(NodeType));
        memcpy(node->payload, (void*)payload, sizeof(NodeType));
        node->next = nullptr;

        uint16_t cachedTailCounter;
        uint64_t cachedTail;
        NodeType *cachedTailPtr;

        while(true){
            cachedTail = rear; // Cache the pointer and counter
            cachedTailPtr = unpack_ptr(cachedTail);
            cachedTailCounter = unpack_counter(cachedTail);
    
            uint64_t packedNext =  reinterpret_cast<uint64_t>(cachedTailPtr->next);
    
            NodeType *nextNode = unpack_ptr(packedNext);

            if(cachedTail == rear){
                
                if(nextNode == nullptr){ //Was tail pointing to the last node?
                    cachedTailPtr->next = reinterpret_cast<NodeType*>(pack(node, cachedTailCounter+1));
                    break;
                } else{
                    rear = pack(nextNode, cachedTailCounter+1);
                }
            }
        }
        rear = pack(node, cachedTailCounter+1);
        
    }

    NodeType* dequeue() override {
   
        NodeType *cachedHeadPointer;
        while (true)
        {
            uint64_t cachedHead = front;
            uint64_t cachedTail = rear;
    
            cachedHeadPointer = unpack_ptr(cachedHead);
            NodeType *cachedTailPointer = unpack_ptr(cachedTail);
    
            uint64_t packedNext =  reinterpret_cast<uint64_t>(cachedHeadPointer->next);
            NodeType *nextNode = unpack_ptr(packedNext);
    
            uint64_t cachedTailCounter = unpack_counter(cachedTail);
            uint64_t cachedHeadCounter = unpack_counter(cachedHead);
   
            if(cachedHead == front){
                if(cachedHeadPointer == cachedTailPointer){
                    if(nextNode == nullptr){
                        return nullptr;
                    }
                  rear = pack(nextNode,cachedTailCounter+1);
                } else{
                    front = pack(nextNode,cachedHeadCounter+1);
                    break;
                }
            }
        }
        free(cachedHeadPointer);
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
std::unique_ptr<AbstractQueue> make_queue(const char type, size_t payload_size) {
    if (type == 'A' && payload_size == 64)
        return std::make_unique<QueueWrapper<Node<64-8>>>(new QueueA<Node<64-8>>());
    if (type == 'A' && payload_size == 128)
        return std::make_unique<QueueWrapper<Node<128-8>>>(new QueueA<Node<128-8>>());
    if (type == 'A' && payload_size == 256)
        return std::make_unique<QueueWrapper<Node<256-8>>>(new QueueA<Node<256-8>>());
    
    if (type == 'B' && payload_size == 64)
        return std::make_unique<QueueWrapper<Node<64-8>>>(new QueueB<Node<64-8>>());
    if (type == 'B' && payload_size == 128)
        return std::make_unique<QueueWrapper<Node<128-8>>>(new QueueB<Node<128-8>>());
    if (type == 'B' && payload_size == 256)
        return std::make_unique<QueueWrapper<Node<256-8>>>(new QueueB<Node<256-8>>());


    if (type == 'C' && payload_size == 64)
        return std::make_unique<QueueWrapper<LockFreeNode<64-8>>>(new QueueC<LockFreeNode<64-8>>());
    if (type == 'C' && payload_size == 128)
        return std::make_unique<QueueWrapper<LockFreeNode<128-8>>>(new QueueC<LockFreeNode<128-8>>());
    if (type == 'C' && payload_size == 256)
        return std::make_unique<QueueWrapper<LockFreeNode<256-8>>>(new QueueC<LockFreeNode<256-8>>());
    return nullptr;
}



bool isReadOnly(serialized_app_command *serializedAppCommand){
    return false;
}



void processRequest(serialized_app_command *serializedAppCommand){
    AbstractQueue *q = static_cast<AbstractQueue*>(serializedAppCommand->arg1);

    if(serializedAppCommand->op_type == SM_OP_ENQUEUE){
        char *payload = static_cast<char*>(serializedAppCommand->arg2);
        q->enqueue(payload);
    } else if(serializedAppCommand->op_type == SM_OP_DEQUEUE){
        serializedAppCommand->responsePtr = q->dequeue();
    } else{
        printf("Unknown command\n");
        exit(EXIT_FAILURE);
    }
}


void benchmark_queue(int numThreads, int opsPerThread, AbstractQueue* q, size_t payload_size) {
    const int totalOps = numThreads * opsPerThread;
    printf("Number of threads %d\n", numThreads);
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    
    for (int i = 0; i < numThreads; i++) {

        workers.emplace_back([=]() {
            mangosteen_args *mangosteenArgs;
            initialise_mangosteen(mangosteenArgs);
            my_rand::init(i);

            char buffer[payload_size];
            
            serializedAppCommand.arg1 = q;
            serializedAppCommand.arg2 = buffer;
            

            for (int j = 0; j < opsPerThread; ++j) {
                int add_or_remove = my_rand::get_rand()%100;
                if(add_or_remove < 55){
                    serializedAppCommand.op_type = SM_OP_ENQUEUE;
                }
                else{
                    serializedAppCommand.op_type = SM_OP_DEQUEUE;
                }
                clientCmd(&serializedAppCommand);
            }
        });
    }

    for (auto& t : workers) t.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

    printf("Node size : %ld\n", payload_size);
    printf("Total operations: %d\n", totalOps);
    printf("Elapsed time: %.6f seconds\n", elapsedSec);
    printf("Throughput: %.2f ops/sec\n", totalOps / elapsedSec);
}

int main(int argc, char** argv) {

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <queue_type> <num_threads> <payload_size>\n", argv[0]);
        return 1;
    }

    const char queue_type = argv[1][0];
    int num_threads = atoi(argv[2]);
    int payload_size = atoi(argv[3]);

    auto queue = make_queue(queue_type, payload_size);
    if (!queue) {
        std::cerr << "Invalid queue type or payload size\n";
        return 1;
    }

    AbstractQueue* q = queue.get();

    printf("Populating queue\n");

    char buf[payload_size];
    for(int i=0; i < 128; i++){
        q->enqueue(buf);
    }
    
    //Mangosteen initialization
    mangosteen_args mangosteenArgs;
    mangosteenArgs.isReadOnly = &isReadOnly;
    mangosteenArgs.processRequest = &processRequest;
    mangosteenArgs.mode = MULTI_THREAD;

    initialise_mangosteen(&mangosteenArgs);
    printf("Mangosteen has initialized\n");


    benchmark_queue(num_threads,500000, q, payload_size);
    return 0;
}
