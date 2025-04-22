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
#define PAYLOAD_SIZE 128

std::mutex headMutex;
std::mutex tailMutex;

thread_local serialized_app_command serializedAppCommand;

typedef struct Node {
    char data[PAYLOAD_SIZE];
    struct Node* next;
} Node;

typedef struct {
    Node* front;
    Node* rear;
    size_t payload_size;
} Queue;

typedef struct QueueCommand {
    char cmdType;
    char *valueToStore;
    char *retrievedValue;
    Queue *queue;
} QueueCommand;

Queue* createQueue(size_t payload_size) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    q->payload_size = payload_size;
    return q;
}

void enqueue(Queue* q, const char* str) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    memcpy(newNode->data, (void*)str, q->payload_size - 1);
    newNode->data[q->payload_size - 1] = '\0';
    newNode->next = NULL;

    std::lock_guard<std::mutex> lock(tailMutex);
    q->rear->next = newNode;
    q->rear = newNode;
    std::lock_guard<std::mutex> unlock(tailMutex);
}

Node* dequeue(Queue* q) {
    std::lock_guard<std::mutex> lock(headMutex);
    Node* node = q->front;
    Node *new_head = node->next;

    if (new_head == NULL) {
        std::lock_guard<std::mutex> unlock(headMutex);
        return NULL;
    }
    q->front = new_head;
    std::lock_guard<std::mutex> unlock(headMutex);
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
            initialise_mangosteen(mangosteenArgs);

            my_rand::init(i);

            char buffer[PAYLOAD_SIZE];
            
            serializedAppCommand.arg1 = q;
            serializedAppCommand.arg2 = buffer;
            

            for (int j = 0; j < opsPerThread; ++j) {
                int add_or_remove = my_rand::get_rand()%2;
                if(add_or_remove == 1){
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

    initialise_mangosteen(&mangosteenArgs);
    printf("Mangosteen has initialized\n");
    
    benchmark_queue(numberOfThreads,3000000, q);
    return 0;
}
