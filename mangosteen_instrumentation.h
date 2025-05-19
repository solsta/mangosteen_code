//
// Created by se00598 on 20/07/23.
//
#ifdef __cplusplus
#include <atomic>
extern "C" {
#endif

#ifndef MANGOSTEEN_INSTRUMENTATION_MANGOSTEEN_INSTRUMENTATION_H
#define MANGOSTEEN_INSTRUMENTATION_MANGOSTEEN_INSTRUMENTATION_H


#ifdef __cplusplus
    typedef std::atomic<int> atomic_int_t;
#else
    #include <stdatomic.h>
    typedef _Atomic int atomic_int_t;
#endif

#include <pthread.h>
#include <strings.h>
#include <assert.h>
#include <signal.h>
#include "stdbool.h"
//#include <stdatomic.h>

#define mangosteenAllocatorArenaSize 2000
#define SINGLE_THREAD 0
#define MULTI_THREAD 1
#define RPC 2

pthread_key_t per_thread_key;

/// APP SPECIFIC START
typedef struct app_thread_args{
    int thread_id;
    void *linkedList;
    void *(*transientAlloc)(size_t);
    void *alloc_handle;
}app_thread_args;

typedef struct serialized_app_command {
    app_thread_args *threadArgs;
    int op_type;
    int numberOfArgs;
    void *arg1;
    void *arg2;
    void *arg3;
    bool response;
    void *responsePtr;
    char serialized_response[32];
    int key;
    //uint64_t *argsBuff;
} serialized_app_command;


/// APP SPECIFIC END

struct thread_entry {
    atomic_int_t ready;
    //volatile int ready;
    char arr[124];
    //client *client;
    char arr2[120];
    char command_request[2048];
    void *applicationSpecificStruct;
    atomic_int_t command_length;
    int thread_number; //4
    atomic_int_t connection_file_descriptor; //4
    pthread_cond_t cv;//48
    pthread_cond_t rv;//48
    pthread_mutex_t cp;//40
    pthread_mutex_t rp;//40
    atomic_int_t connection_slot;//8
    bool is_recovering;
    struct pmemobjpool *pop;
    struct ring_buffer *rb;
    char arr3[40];
    int listenfd;
    int epollfd;
    int client_connections;

    //unsigned int *shared_counter;
    void *(*transientAlloc)(size_t);

    void (*transientFree)(void *);

    void *handle;

    void (*mallctl1)(char *, void *, void *, void *, size_t);
    //deferred_mmap_entries deferredMmapEntries;
};

typedef struct mangosteenMemoryAllocator {
    int offset;
    char threadLocalArena[mangosteenAllocatorArenaSize];
} mangosteen_memory_allocator;

typedef struct genericResponse {
    void *response;
    int size;
} generic_response;

extern void mangosteenResetMemoryAllocator(mangosteen_memory_allocator *mma);
extern void *mangosteenAllocateMemory(mangosteen_memory_allocator *mma, int size);

typedef struct mangosteen_args {
    bool (*isReadOnly)(serialized_app_command *serializedAppCommand);
    void (*loadServiceRequest)(serialized_app_command *serializedAppCommand, char*);
    void (*processRequest)(serialized_app_command *serializedAppCommand);
    void *(*allocateThreadLocalServiceRequestStruct)(void);
    void *freeThreadLocalServiceRequestStruct;
    void (*getResponseInfo)(serialized_app_command *serializedAppCommand, generic_response *genericResponse);
    void (*enableCombiner)(void);
    void (*disableCombiner)(void);
    void *resetAllocator;
    void (*setPerThreadLocalThreadData)(void* (*)(size_t), void (*)(void*));
    int mode;
} mangosteen_args;

extern void mangosteen_initialize_thread();
void initialise_mangosteen(mangosteen_args *mangosteenArgs);
void execute_using_flat_combining_no_rpc(serialized_app_command *serializedAppCommand);
void clientCmd(serialized_app_command *serializedAppCommand);
#endif //MANGOSTEEN_INSTRUMENTATION_MANGOSTEEN_INSTRUMENTATION_H
#ifdef __cplusplus
}
#endif
