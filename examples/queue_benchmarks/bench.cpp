#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"
#include "/home/se00598/home/se00598/vanila_flit/flit/common/rand_r_32.h"
#include <jemalloc/jemalloc.h>
#include <thread>
#include <chrono>
#include <vector>
#include <iostream>
#include <string>
#include <memory>
#include <string.h>

// ---------------------- Mongosteen code ----------------------
thread_local serialized_app_command serializedAppCommand;
#define SM_OP_ENQUEUE 0
#define SM_OP_DEQUEUE 1

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

NodeType* front;
NodeType* rear;

QueueA () {
    NodeType *node = (NodeType*)malloc(sizeof(NodeType));
    node->next = NULL;
    front = rear = node;
}
    void enqueue(const char* payload) override {
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

    NodeType* dequeue() override {
        NodeType* node = front;
        NodeType *new_head = node->next;
        if (new_head == NULL) {
            return NULL;
        }
        printf("Node payload: %s\n", new_head->payload);
        front = new_head;
        free(node);
        return nullptr;
    }
};

// ---------------------- QueueB implementation ----------------------
template <typename NodeType>
class QueueB : public IQueue<NodeType> {
public:
    void enqueue(const char* payload) override {
        std::cout << "[QueueB] Enqueue: " << payload << "\n";
    }

    NodeType* dequeue() override {
        std::cout << "[QueueB] Dequeue\n";
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
    
    if (type == "QueueB" && payload_size == 64)
        return std::make_unique<QueueWrapper<Node<64>>>(new QueueB<Node<64>>());
    if (type == "QueueB" && payload_size == 128)
        return std::make_unique<QueueWrapper<Node<128>>>(new QueueB<Node<128>>());
    if (type == "QueueB" && payload_size == 256)
        return std::make_unique<QueueWrapper<Node<248>>>(new QueueB<Node<248>>());
    
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

    printf("Node size : %d\n", payload_size + 8);
    printf("Total operations: %d\n", totalOps);
    printf("Elapsed time: %.6f seconds\n", elapsedSec);
    printf("Throughput: %.2f ops/sec\n", totalOps / elapsedSec);
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

    AbstractQueue* q = queue.get();

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


    benchmark_queue(30,50000, q, payload_size);
    return 0;
}
