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
#include "node.h"
#include "IQueue.h"
#include "QueueA.h"
#include "QueueAWrapper.h"

#define SM_OP_ENQUEUE 0
#define SM_OP_DEQUEUE 1
#define RUN_WITH_MANGOSTEEN

std::mutex headMutex;
std::mutex tailMutex;


/*
void processRequest(serialized_app_command *serializedAppCommand){
    QueueLockBased *q = static_cast<QueueLockBased*>(serializedAppCommand->arg1);

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

            char buffer[128];
            
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

    printf("Total operations: %d\n", totalOps);
    printf("Elapsed time: %.6f seconds\n", elapsedSec);
    printf("Throughput: %.2f ops/sec\n", totalOps / elapsedSec);
}
*/
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <number>\n", argv[0]);
        return 1;
    }

    int numberOfThreads = atoi(argv[1]);

    using benchNode = Node<120>;
    IQueue<benchNode> *q = new QueueAWrapper<benchNode>();

    char buf[128];
    
    benchNode* n = static_cast<benchNode*>(malloc(sizeof(benchNode)));
    memcpy(n->payload,"Hello\n", 6);
    q->enqueue(n);
    benchNode* nt = static_cast<benchNode*>(malloc(sizeof(benchNode)));
    memcpy(nt->payload,"world!\n", 6);
    q->enqueue(nt);
    printf("%s", q->dequeue()->payload);
    printf("%s", q->dequeue()->payload);

    
    /*
        for(int i=0; i < 128; i++){
        q->enqueue(buf);
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
    benchmark_queue(numberOfThreads,500000, q);
    return 0;
    */
}
