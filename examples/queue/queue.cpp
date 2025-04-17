#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"
#include <jemalloc/jemalloc.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <cstdio>  
#include <thread>
#include <chrono>

#define SM_OP_ENQUEUE 0
#define SM_OP_DEQUEUE 1

#define PAYLOAD_SIZE 32

// Node structure with dynamically allocated payload
typedef struct Node {
    int id;
    char data[PAYLOAD_SIZE];
    struct Node* next;
} Node;

// Queue structure that includes payload size info
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

// Create a new queue with specified payload size
Queue* createQueue(size_t payload_size) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) {
        fprintf(stderr, "Failed to allocate memory for queue\n");
        exit(EXIT_FAILURE);
    }
    q->front = q->rear = NULL;
    q->payload_size = payload_size;
    return q;
}

// Check if the queue is empty
int isEmpty(Queue* q) {
    return q->front == NULL;
}

// Enqueue a new string with dynamic payload size
void enqueue(Queue* q, const char* str) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) {
        fprintf(stderr, "Failed to allocate memory for node\n");
        exit(EXIT_FAILURE);
    }
    newNode->id = 1;
    memcpy(newNode->data, (void*)str, q->payload_size - 1);
    newNode->data[q->payload_size - 1] = '\0';
    newNode->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

char response[64];

// Dequeue and return a copy of the data
Node* dequeue(Queue* q) {
    //fprintf(stderr, "Deque start\n");
    if (isEmpty(q)) {
        //strncpy(response, "Queue is empty\n", strlen("Queue is empty\n"));
        return NULL;
    }
    //fprintf(stderr, "Queue is not empty\n");
    //fprintf(stderr, "Dequing: %d\n", q->front->id);
    fprintf(stderr, "Dequing: %s\n", q->front->data);
    Node* responceNode = q->front;
    //strncpy(response, q->front->data, q->payload_size);
    

    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }

    //free(temp->data);
    //free(temp);

    return responceNode;
}

// Free the queue and all nodes
void freeQueue(Queue* q) {
    Node* current = q->front;
    while (current != NULL) {
        Node* next = current->next;
        free(current->data);
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

thread_local serialized_app_command serializedAppCommand;

std::atomic<int> thread_id_counter{0};

char entry[PAYLOAD_SIZE];
void runBenchmarkThread(void *arg){
    Queue *q = static_cast<Queue*>(arg);
    //std::this_thread::sleep_for(std::chrono::seconds(thread_id_counter.fetch_add(1)));
    

    char buffer[PAYLOAD_SIZE];

    for(int i=0; i < 9; i++){
        int my_id = thread_id_counter.fetch_add(1);
        std::snprintf(buffer, sizeof(buffer), "msg from %d", my_id);
        //printf("Thread id %d\n", my_id);
        
        serializedAppCommand.op_type = SM_OP_ENQUEUE;
        serializedAppCommand.arg1 = q;
        serializedAppCommand.arg2 = buffer;
        clientCmd(&serializedAppCommand);
    }


    
    std::this_thread::sleep_for(std::chrono::seconds(1));
   printf("Dequeue success\n");
}

void *initAndRunBenchMarkThread(void *arg){
    mangosteen_args *mangosteenArgs;
    initialise_mangosteen(mangosteenArgs);
    //printf("thread initialized\n");
    runBenchmarkThread(arg);
    return NULL;
}

int main() {
    int number_of_threads =2;
    size_t payload_size = PAYLOAD_SIZE;
    Queue* q = createQueue(payload_size);
    
    //Mangosteen initialization
    mangosteen_args mangosteenArgs;
    mangosteenArgs.isReadOnly = &isReadOnly;
    mangosteenArgs.processRequest = &processRequest;
    mangosteenArgs.mode = MULTI_THREAD;

    initialise_mangosteen(&mangosteenArgs);
    printf("Mangosteen has initialized\n");
    printf("About to dequeue\n");
    printf("In the queue: %s", dequeue(q));

    
    /*
    char entry[PAYLOAD_SIZE];
    memcpy(entry, "abcdefghijklmnop", 16);
    printf("About to enqueue ?\n");
    instrument_start();
    enqueue(q,&entry[0]);
    instrument_stop();
*/
    
    pthread_t threads[number_of_threads];
    for (int i = 0; i < number_of_threads; i++) {
        pthread_create(&threads[i], NULL, initAndRunBenchMarkThread, q);
    }
    
    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    freeQueue(q);
    
    return 0;
}
