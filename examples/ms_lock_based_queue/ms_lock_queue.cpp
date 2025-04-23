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
#define PAYLOAD_SIZE 16
#define RUN_WITH_MANGOSTEEN

std::mutex headMutex;
std::mutex tailMutex;

thread_local serialized_app_command serializedAppCommand;

typedef struct Node {
    int id;
    char data[PAYLOAD_SIZE];
    struct Node* next;
} Node;

typedef struct {
    Node* front;
    Node* rear;
} Queue;

typedef struct QueueCommand {
    char cmdType;
    char *valueToStore;
    char *retrievedValue;
    Queue *queue;
} QueueCommand;

Queue* createQueue(size_t payload_size) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    Node *node = (Node*)malloc(sizeof(Node));
    node->next = NULL;
    q->front = q->rear = node;
    return q;
}

void enqueue(Queue* q, const char* str) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->id = 1;
    memcpy(newNode->data, (void*)str, PAYLOAD_SIZE - 1);
    //instrumented_memcpy(newNode->data, (void*)str, PAYLOAD_SIZE - 1);
    newNode->data[PAYLOAD_SIZE - 1] = '\0';
    newNode->next = NULL;
 #ifndef RUN_WITH_MANGOSTEEN
    tailMutex.lock();
#endif
    q->rear->next = newNode;
    q->rear = newNode;
#ifndef RUN_WITH_MANGOSTEEN
    tailMutex.unlock();
#endif
}

Node* dequeue(Queue* q) {
#ifndef RUN_WITH_MANGOSTEEN
    headMutex.lock();
#endif
        Node* node = q->front;
        Node *new_head = node->next;

        if (new_head == NULL) {
#ifndef RUN_WITH_MANGOSTEEN
            headMutex.unlock();
#endif
            return NULL;
        }
        q->front = new_head;
#ifndef RUN_WITH_MANGOSTEEN
    headMutex.unlock();
#endif
    free(node);
    return NULL;
}

void freeQueue(Queue* q) {
    Node* current = q->front;
    while (current != NULL) {
        Node* next = current->next;
        free(current);
        current = next;
    }
    free(q);
}

bool isReadOnly(serialized_app_command *serializedAppCommand){
    return false;
}



void processRequest(serialized_app_command *serializedAppCommand){
    Queue *q = static_cast<Queue*>(serializedAppCommand->arg1);

    if(serializedAppCommand->op_type == SM_OP_ENQUEUE){
        char *payload = static_cast<char*>(serializedAppCommand->arg2);
        enqueue(q, payload);
    } else if(serializedAppCommand->op_type == SM_OP_DEQUEUE){
        serializedAppCommand->responsePtr = dequeue(q);
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
                    enqueue(q, buffer);
#endif
                }
                else{
#ifdef RUN_WITH_MANGOSTEEN
                    serializedAppCommand.op_type = SM_OP_DEQUEUE;
#else
                    dequeue(q);
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
    Queue* q = createQueue(payload_size);
    char buf[PAYLOAD_SIZE];
    for(int i=0; i < 128; i++){
        enqueue(q,buf);
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
    benchmark_queue(numberOfThreads,100000, q);
    return 0;
}
