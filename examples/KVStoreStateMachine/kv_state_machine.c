#include <string.h>
#include <sys/mman.h>
#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"
#include "kv_state_machine.h"
#include "stdbool.h"
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

char *store;
int number_of_threads = 1;
void putValueAtIndex(kvClient *c);

void print_kv_elements(){
    for(int i=0; i < STORE_SZ*VAL_SZ; i+=VAL_SZ){
        if(store[i] != 0){
            char val[VAL_SZ];
            memcpy(val, &store[i], VAL_SZ);
            printf("el %d : %s\n", i, val);
        }
    }
}

void *runWorkerThread(void *arg);
/* Existing unmodified app state machine code */
void initKVStoreStateMachine() {
    store = malloc(STORE_SZ*VAL_SZ);
    bzero(store, STORE_SZ*VAL_SZ);
}

void getValueAtIndex(kvClient *c){
    memcpy(c->response, &store[c->currCmd->index*VAL_SZ], VAL_SZ);
}

void putValueAtIndex(kvClient *c){
    printf("putting element at index: %d\n", c->currCmd->index);
    printf("real offset: %d\n", c->currCmd->index*VAL_SZ);
    memcpy(&store[c->currCmd->index*VAL_SZ], c->currCmd->value, VAL_SZ);
    printf("put worked ok\n");
    memcpy(c->response, "OK",2);
}

void handleRequest(kvClient *c) {
    if(c->currCmd->type == PUT){
        putValueAtIndex(c);
    }
    else{
        getValueAtIndex(c);
    }
}

/* KV store implementations of Mangosteen API callbacks */
// Called for every new client connection
void *initAppClient(void *params){
    kvClient* c = malloc(sizeof(kvClient));
    c->id = *(int*)params; // e.g. connection fd
    return c;  //opaque client ctx
}

// Called after client connection is closed
void destroyAppClient(void *opaqueCtx){
    free(opaqueCtx);
}

// Called for every command
void deserializeRequest(void *opaqueCtx, void *raw_req){
    ((kvClient*)opaqueCtx)->currCmd = (kvCmd*)raw_req;
}

// Called for every command
bool isReadOnly(void *opaqueCtx){
    printf("in read only\n");
    if(((kvClient*)opaqueCtx)->currCmd->type == PUT){
        return false;
    }
    return true;
}

// Called for every command.
// Mangosteen will call exec_ro or exec_rw depending on
// the return value of isReadOnly.
void processRequest(void *opaqueCtx){
    handleRequest((kvClient*)opaqueCtx);
}

void* getResponse(void *opaqueCtx) {
    kvClient *c = (kvClient*)opaqueCtx;
    mangoResponse *mResponse = malloc(sizeof(mangoResponse));
    mResponse->response = c->response;
    mResponse->size = VAL_SZ;
    return mResponse;
}

void *initAndRunWorkerThread(void *arg){
    mangosteen_args mangosteenArgs;
    mangosteenArgs.isReadOnly = &isReadOnly;
    mangosteenArgs.allocateThreadLocalServiceRequestStruct = &initAppClient;
    mangosteenArgs.loadServiceRequest = &deserializeRequest;
    mangosteenArgs.processRequest = &processRequest;
    mangosteenArgs.getResponseInfo = &getResponse;
    mangosteenArgs.freeThreadLocalServiceRequestStruct = &destroyAppClient;
    mangosteenArgs.mode = MULTI_THREAD;

    initialise_mangosteen(&mangosteenArgs);

    runWorkerThread(arg);
}

void *runWorkerThread(void *arg){
	printf("In worker thread\n");
    kvClient kvClient1;

    kvClient1.currCmd = malloc(sizeof (kvCmd));
    kvClient1.currCmd->type = PUT;
    kvClient1.currCmd->index = 0;
    memcpy(kvClient1.currCmd->value, "abcdefghijklmnop", VAL_SZ);
    clientCmd(&kvClient1);
    printf("thread done\n");
    return NULL;
}

int main() {
    // Application specific
    initKVStoreStateMachine();

    //Mangosteen initialization (paper)
    mangosteen_args mangosteenArgs;
    mangosteenArgs.isReadOnly = &isReadOnly;
    mangosteenArgs.allocateThreadLocalServiceRequestStruct = &initAppClient;
    mangosteenArgs.loadServiceRequest = &deserializeRequest;
    mangosteenArgs.processRequest = &processRequest;
    mangosteenArgs.getResponseInfo = &getResponse;
    mangosteenArgs.freeThreadLocalServiceRequestStruct = &destroyAppClient;
    mangosteenArgs.mode = MULTI_THREAD;

    initialise_mangosteen(&mangosteenArgs);

    printf("Elements in the list:\n");
    print_kv_elements();
    printf("Elements list finished\n");
	
	    printf("Elements in the list:\n");
    print_kv_elements();
    printf("Elements list finished\n");

    pthread_t threads[number_of_threads];
    for (int i = 0; i < number_of_threads; i++) {
        pthread_create(&threads[i], NULL, initAndRunWorkerThread, NULL);
    }

    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }


    printf("Elements in the list:\n");
    print_kv_elements();
    printf("Elements list finished\n");

    return 0;
}
