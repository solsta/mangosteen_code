#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"
#include <jemalloc/jemalloc.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <cstdio>  
#include <thread>
#include <chrono>
#include "/home/se00598/home/se00598/vanila_flit/flit/common/rand_r_32.h"
#include <vector>
#include <mutex>

#define SM_OP_ENQUEUE 0
#define SM_OP_DEQUEUE 1
#define PAYLOAD_SIZE 120
#define RUN_WITH_MANGOSTEEN

thread_local serialized_app_command serializedAppCommand;

constexpr uint64_t POINTER_MASK = 0x0000FFFFFFFFFFFF;  // Lower 48 bits
constexpr uint64_t COUNTER_MASK = 0xFFFF000000000000;  // Upper 16 bits

class Node;
class Queue;

uint64_t pack(Node* ptr, uint16_t counter);
Node* unpack_ptr(uint64_t packed);
uint16_t unpack_counter(uint64_t packed);
void enqueue(Queue *q, char *payload, int payloadSize);
bool dequeue(Queue *q, char *response);

class Pointer{
public:
Node *ptr;
uint16_t counter;


    Pointer(Node *ptr, int counter){
       this->ptr = ptr;
       this->counter = counter;
    }
    bool operator==(const Pointer& other) const {
        return ptr == other.ptr && counter == other.counter;
    }
    void setPointer(Node *ptr) const{
        ptr = reinterpret_cast<Node*>(reinterpret_cast<uint64_t>(ptr) & POINTER_MASK);
    }
    void setCounter(uint16_t newValue) const{
        
    }
};


class Node{

public:
    std::atomic<uint64_t> next;
    char nodePayload[PAYLOAD_SIZE];
    Node(char *payload, int payloadSize){
        assert(payloadSize <= PAYLOAD_SIZE);
        memcpy(&nodePayload, payload, payloadSize);
        next = pack(NULL, 0);
    }
};


class Queue{
public:
    std::atomic<uint64_t> head; // This is actually a packed Pointer
    std::atomic<uint64_t> tail;

    Queue(){
        Node *node = new Node("", strlen(""));
        uint64_t nodePtr = pack(node,0);
        head = tail = nodePtr;
    }
};

uint64_t pack(Node* ptr, uint16_t counter) {
    uint64_t ptrBits = reinterpret_cast<uint64_t>(ptr) & POINTER_MASK;
    uint64_t countBits = static_cast<uint64_t>(counter) << 48;
    return ptrBits | countBits;

}

Node* unpack_ptr(uint64_t packed) {
    return reinterpret_cast<Node*>(packed & POINTER_MASK);

}

uint16_t unpack_counter(uint64_t packed) {
    return static_cast<uint16_t>(packed >> 48);

}

void enqueue(Queue *q, char *payload, int payloadSize){
    Node *node = new Node(payload, payloadSize);
    uint16_t cachedTailCounter;
    uint64_t cachedTail;
    Node *cachedTailPtr;
    while(true){
        cachedTail = q->tail; // Cache the pointer and counter
        cachedTailPtr = unpack_ptr(cachedTail);
        cachedTailCounter = unpack_counter(cachedTail);

        uint64_t packedNext =  cachedTailPtr->next;

        Node *nextNode = unpack_ptr(packedNext);

        if(cachedTail == q->tail){
            if(nextNode == NULL){ //Was tail pointing to the last node?

#ifdef RUN_WITH_MANGOSTEEN
bool res = cachedTailPtr->next.compare_exchange_strong(packedNext, pack(node, cachedTailCounter+1));
if(res){
    break;
}
#else

                bool res = cachedTailPtr->next.compare_exchange_strong(packedNext, pack(node, cachedTailCounter+1));
                if(res){
                    break;
                }
#endif
            } else{
#ifdef RUN_WITH_MANGOSTEEN
q->tail.compare_exchange_strong(cachedTail, pack(nextNode, cachedTailCounter+1));
#else
                q->tail.compare_exchange_strong(cachedTail, pack(nextNode, cachedTailCounter+1));
#endif
            }
        }
    }
#ifdef RUN_WITH_MANGOSTEEN
q->tail.compare_exchange_strong(cachedTail, pack(node, cachedTailCounter+1));
#else
    q->tail.compare_exchange_strong(cachedTail, pack(node, cachedTailCounter+1));
#endif

}

char res[PAYLOAD_SIZE];
bool dequeue(Queue *q, char *response){
    Node *cachedHeadPointer;
    while (true)
    {
        uint64_t cachedHead = q->head;
        uint64_t cachedTail = q->tail;

        cachedHeadPointer = unpack_ptr(cachedHead);
        Node *cachedTailPointer = unpack_ptr(cachedTail);

        uint64_t packedNext =  cachedHeadPointer->next;
        Node *nextNode = unpack_ptr(packedNext);

        uint64_t cachedTailCounter = unpack_counter(cachedTail);
        uint64_t cachedHeadCounter = unpack_counter(cachedHead);

        if(cachedHead == q->head){
            if(cachedHeadPointer == cachedTailPointer){
                if(nextNode == NULL){
                    return false;
                }
#ifdef RUN_WITH_MANGOSTEEN
q->tail.compare_exchange_strong(cachedTail,pack(nextNode,cachedTailCounter+1));
#else
              q->tail.compare_exchange_strong(cachedTail,pack(nextNode,cachedTailCounter+1));
#endif
            } else{
#ifdef RETRUN_RESULT_FOR_DEQUEUE
                memcpy(&res, nextNode->nodePayload, PAYLOAD_SIZE); // This corresponds to line D12
#endif


#ifdef RUN_WITH_MANGOSTEEN
nextNode->nodePayload;
if(q->head.compare_exchange_strong(cachedHead,pack(nextNode,cachedHeadCounter+1))){
    break;
}
#else
                nextNode->nodePayload;
                if(q->head.compare_exchange_strong(cachedHead,pack(nextNode,cachedHeadCounter+1))){
                    break;
                }
#endif
            }
        }
    }
    delete cachedHeadPointer;
    return true;

}

bool isReadOnly(serialized_app_command *serializedAppCommand){
    return false;
}



void processRequest(serialized_app_command *serializedAppCommand){
    Queue *q = static_cast<Queue*>(serializedAppCommand->arg1);

    if(serializedAppCommand->op_type == SM_OP_ENQUEUE){
        char *payload = static_cast<char*>(serializedAppCommand->arg2);
        enqueue(q, payload, PAYLOAD_SIZE);
    } else if(serializedAppCommand->op_type == SM_OP_DEQUEUE){
        dequeue(q, NULL);
    } else{
        printf("Unknown command\n");
        exit(EXIT_FAILURE);
    }
}

void benchmark_queue(int numThreads, int opsPerThread, Queue* q) {
    const int totalOps = numThreads * opsPerThread;
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    for (int i = 0; i < numThreads; i++) {

        workers.emplace_back([=]() {
            mangosteen_args *mangosteenArgs;
#ifdef RUN_WITH_MANGOSTEEN
            initialise_mangosteen(mangosteenArgs);
#endif
            my_rand::init(i);

            char buffer[PAYLOAD_SIZE];
            
            serializedAppCommand.arg1 = q;
            serializedAppCommand.arg2 = buffer;
            

            for (int j = 0; j < opsPerThread; ++j) {
                int add_or_remove = my_rand::get_rand()%2;
                if(add_or_remove == 1){
#ifdef RUN_WITH_MANGOSTEEN
                    serializedAppCommand.op_type = SM_OP_ENQUEUE;
#else
                    enqueue(q, buffer, PAYLOAD_SIZE);
#endif
                }
                else{
#ifdef RUN_WITH_MANGOSTEEN
                    serializedAppCommand.op_type = SM_OP_DEQUEUE;
#else
                    char resp_buf[PAYLOAD_SIZE];
                    dequeue(q, resp_buf);
#endif
                }
#ifdef RUN_WITH_MANGOSTEEN
                clientCmd(&serializedAppCommand);
#endif
            }
        });
    }

    for (auto& t : workers) t.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

    printf("Total operations: %d\n", totalOps);
    printf("Elapsed time: %.6f seconds\n", elapsedSec);
    printf("Throughput: %.2f ops/sec\n", totalOps / elapsedSec);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <number>\n", argv[0]);
        return 1;
    }

    int numberOfThreads = atoi(argv[1]);
    size_t payload_size = PAYLOAD_SIZE;
    Queue *q = new Queue();

    char buf[PAYLOAD_SIZE];
    for(int i=0; i < 128; i++){
        enqueue(q,buf, PAYLOAD_SIZE);
    }
    
    //Mangosteen initialization
    mangosteen_args mangosteenArgs;
    mangosteenArgs.isReadOnly = &isReadOnly;
    mangosteenArgs.processRequest = &processRequest;
    mangosteenArgs.mode = MULTI_THREAD;
#ifdef RUN_WITH_MANGOSTEEN
    initialise_mangosteen(&mangosteenArgs);
    printf("Mangosteen has initialized\n");
#endif
    benchmark_queue(numberOfThreads,50000, q);
    return 0;
}
