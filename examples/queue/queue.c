#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Node structure with dynamically allocated payload
typedef struct Node {
    char* data;
    struct Node* next;
} Node;

// Queue structure that includes payload size info
typedef struct {
    Node* front;
    Node* rear;
    size_t payload_size;
} Queue;

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

    newNode->data = (char*)malloc(q->payload_size);
    if (!newNode->data) {
        fprintf(stderr, "Failed to allocate memory for payload\n");
        exit(EXIT_FAILURE);
    }

    strncpy(newNode->data, str, q->payload_size - 1);
    newNode->data[q->payload_size - 1] = '\0';
    newNode->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

// Dequeue and return a copy of the data
char* dequeue(Queue* q) {
    if (isEmpty(q)) {
        char *responseMessage = malloc(64);
        strncpy(responseMessage, "Queue is empty\n", strlen("Queue is empty\n"));
        return responseMessage;
    }

    Node* temp = q->front;
    char* result = (char*)malloc(q->payload_size);
    if (!result) {
        fprintf(stderr, "Failed to allocate memory for return payload\n");
        exit(EXIT_FAILURE);
    }

    strncpy(result, temp->data, q->payload_size);

    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }

    free(temp->data);
    free(temp);

    return result;
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

int main() {
    size_t payload_size = 64;
    Queue* q = createQueue(payload_size);


    char* msg = dequeue(q);
    printf("Dequeued: %s\n", msg);
    free(msg);


    enqueue(q, "Message one");
    enqueue(q, "Another message that's a bit longer");
    enqueue(q, "Short");

    msg = dequeue(q);
    printf("Dequeued: %s\n", msg);
    free(msg);

    msg = dequeue(q);
    printf("Dequeued: %s\n", msg);
    free(msg);

    freeQueue(q);
    return 0;
}
