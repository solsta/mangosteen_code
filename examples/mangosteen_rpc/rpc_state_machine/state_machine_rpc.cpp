//
// Created by se00598 on 13/03/24.
//
#include <iostream>
#include <climits>
#include <jemalloc/jemalloc.h>
#include <dlfcn.h>
#include <cstring>
#include <emmintrin.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "../../../mangosteen_instrumentation.h"
#include "../../../flat_combining/dr_annotations.h"


#define ALIGNMENT 32
#define USE_DLM_LOADED_ALLOCATOR
#define NODE_PAYLOAD_SIZE 32
#define CHECK_ALIGNMENT
#define MULTIPLE_WORKER_THREADS

char *full_string = "It is not the responsibility of knights errant to discover whether the afflicted, \"\n"
                               "                          \"the enchained and the oppressed whom they encounter on the road are reduced to these circumstances \"\n"
                               "                          \"and suffer this distress for their vices, or for their virtues: \"\n"
                               "                          \"the knight's sole responsibility is to succour them as people in need, \"\n"
                               "                          \"having eyes only for their sufferings, not for their misdeeds.\n";

void *allocateMemory(size_t size);
size_t list_size;
size_t slices;


#define SM_OP_ADD 0
#define SM_OP_REMOVE 1
#define SM_OP_CONTAINS 2
#define TODO 3

typedef struct app_per_thread_data{
    bool combinerIsActive;
    void *(*persistentAlloc)(size_t);
}app_per_thread_data;

thread_local app_per_thread_data appPerThreadData;
void *allocator_handle;

void set_per_thread_data(void* (*persistentAllocator)(size_t), void (*persistentFree)(void*)){
    appPerThreadData.combinerIsActive = false;
    appPerThreadData.persistentAlloc = persistentAllocator;
    assert(appPerThreadData.persistentAlloc);
    printf("Per thread data is set\n");
}

void *allocateMemory(size_t size){
    if(appPerThreadData.combinerIsActive){
        return appPerThreadData.persistentAlloc(size);
    }
    return malloc(size);
}

void disableCombiner(){
    appPerThreadData.combinerIsActive = false;
}

void enableCombiner(){
    appPerThreadData.combinerIsActive = true;
}

thread_local serialized_app_command threadLocalSerializedAppCommand; // TODO change this to work with allocator

void *allocate_app_specific_struct(){
    std::printf("ALLOCATE APP SPECIFIC STRUCT\n");
    return allocateMemory(sizeof (serialized_app_command));
}

void load_service_request(serialized_app_command *serializedAppCommand, char *rawCommand){
    std::printf("LOAD SERVICE REQUEST\n");
    std::printf("%s\n", rawCommand);
    char op_type_char = rawCommand[0];
    long value = strtoll(&op_type_char, nullptr, 10);
    printf("Value: %lu\n", value);

    if(serializedAppCommand->op_type == SM_OP_ADD){
        serializedAppCommand->op_type = value;
        serializedAppCommand->arg1;
    }
    if(serializedAppCommand->op_type == SM_OP_REMOVE){
        std::printf("ERROR: NOT IMPLEMENTED\n");
        exit(1);
    }
    if(serializedAppCommand->op_type == SM_OP_CONTAINS){
        std::printf("ERROR: NOT IMPLEMENTED\n");
        exit(1);
    }
}

bool is_read_only(serialized_app_command *serializedAppCommand){
    if(serializedAppCommand->op_type == SM_OP_ADD){
        return false;
    }
    if(serializedAppCommand->op_type == SM_OP_REMOVE){
        return false;
    }
    if(serializedAppCommand->op_type == SM_OP_CONTAINS){
        return true;
    }
    std::printf("ERROR: CLASSIFIER UNKNOWN COMMAND TYPE\n");
    exit(1);
}
#define MEMORY_BLOCK_SIZE 4096
static char *arr;
void initializeStateMachine(){
    const char *file_path = "memoryBlock.txt";
    int fd = open(file_path, O_RDWR | O_CREAT, 0666);
    assert(ftruncate(fd, MEMORY_BLOCK_SIZE)!=-1);
    assert(fd != -1);

    arr = static_cast<char *>(mmap(nullptr, MEMORY_BLOCK_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE, fd, 0));
    bzero(arr,MEMORY_BLOCK_SIZE);
}

bool do_step(){
    size_t seqno = list_size;
    list_size++;
    if(seqno < slices){
        size_t offset = seqno * NODE_PAYLOAD_SIZE;
        char *src = full_string + offset;
        char *dest = arr + offset;
        memcpy(dest, src, NODE_PAYLOAD_SIZE);
        return true;
    }
    return false;
}

void process_command(serialized_app_command *serializedAppCommand){
    if(serializedAppCommand->op_type == SM_OP_ADD){
        serializedAppCommand->response = do_step();
        return;
    }
    if(serializedAppCommand->op_type == SM_OP_REMOVE){
        std::printf("ERROR: NOT IMPLEMENTED\n");
        exit(1);
    }
    if(serializedAppCommand->op_type == SM_OP_CONTAINS){
        std::printf("ERROR: NOT IMPLEMENTED\n");
        exit(1);
    }
    std::printf("ERROR: CLASSIFIER UNKNOWN COMMAND TYPE\n");
    exit(1);
}

void get_responce(serialized_app_command *serializedAppCommand, genericResponse *genericResponse){
    std::printf("IN GET RESPONSE\n");
    printf("%s\n", arr);

    if(serializedAppCommand->response){
        serializedAppCommand->serialized_response[0] = '0';
        genericResponse->response = serializedAppCommand->serialized_response;
        genericResponse->size = 1;
    }else{
        serializedAppCommand->serialized_response[0] = '1';
        genericResponse->response = serializedAppCommand->serialized_response;
        genericResponse->size = 1;
    }
}

int main() {
    list_size = 0;
    slices = strlen(full_string) / NODE_PAYLOAD_SIZE;
	
    //allocator_handle = dlmopen(LM_ID_NEWLM, "libjemalloc.so", RTLD_NOW);
    //assert(allocator_handle);


    initializeStateMachine();
    //Mangosteen initialization
    mangosteen_args mangosteenArgs;
    mangosteenArgs.isReadOnly = &is_read_only;
    mangosteenArgs.processRequest = &process_command;
    mangosteenArgs.allocateThreadLocalServiceRequestStruct = &allocate_app_specific_struct;
    mangosteenArgs.loadServiceRequest = &load_service_request;
    mangosteenArgs.disableCombiner = &disableCombiner;
    mangosteenArgs.enableCombiner = &enableCombiner;
    mangosteenArgs.setPerThreadLocalThreadData = &set_per_thread_data;
    mangosteenArgs.freeThreadLocalServiceRequestStruct;
    mangosteenArgs.resetAllocator;
    mangosteenArgs.getResponseInfo = &get_responce;

    mangosteenArgs.mode = RPC;

    initialise_mangosteen(&mangosteenArgs);
    return 0;
}
