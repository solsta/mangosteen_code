//
// Created by se00598 on 10/5/22.
//

#ifndef REDIS_FLAT_COMBINING_H
#define REDIS_FLAT_COMBINING_H

#include <sched.h>
#define _GNU_SOURCE
//#include <pthread.h>
#include <bits/types/sig_atomic_t.h>
#include <pthread.h>
#include "stdbool.h"
//#include "ring_buffer.h"
#include "../ring_buffer/ring_buffer.h"
#include "../mangosteen_instrumentation.h"
pthread_mutex_t mutex;
#define SUCCESS 0
#define NUMBER_OF_THREADS 30
#define THREADS_PER_SINGLE_WM 30
#define WM_ARRAY_SIZE (NUMBER_OF_THREADS/THREADS_PER_SINGLE_WM)
#define PORT_NUMBER 6379
#define CONNECTION_AVAILABLE 0
#define CONNECTION_BUSY 1
#define NOT_READY 0
#define READY_TO_BE_EXECUTED 1
#define READY_TO_REPLY 2
#define STARTED 0
#define DONE 1
#define READY_TO_EXECUTE 2
#define NONE 3
#define READY_TO_INTIALISE 4
//cpu_set_t cpuSet;

//#define MANGOSTEEN_DEBUG 0
//_Atomic unsigned long WM_ARRAY[WM_ARRAY_SIZE];
//_Atomic unsigned long WM;
_Atomic unsigned long WM_indicator;
static volatile int initializedThreads = 0;
static volatile bool recoveryIsComplete = false;
//#define MANGOSTEEN_DEBUG 0

struct thread_entry *taskArray;
extern __thread int thread_index;
//
void configure_cpu_set(struct thread_entry *threadEntry);
void *StartServerThread(void *context);
_Noreturn void *start_worker_thread(void *thread_context);
_Noreturn void *UpdateExecutionThread(void *context);
void spawnThreads(bool recoveringProcess);
void allocateFlatCombiningdMemory();
bool (*isReadOnlyCallBack)(serialized_app_command *serializedAppCommand);
void (*loadServiceRequestCallBack)(serialized_app_command *serializedAppCommand, char*);
void (*processRequestCallBack)(serialized_app_command *serializedAppCommand);
void* (*allocateThreadLocalServiceRequestStructCallBack)(void);
void (*getResponseInfoCallBack)(serialized_app_command *serializedAppCommand,generic_response *genericResponse);
void (*freeThreadLocalServiceRequestStructCallBack)(void *);
void (*enableCombinerCallBack)(void);
void (*disableCombinerCallBack)(void);
void (*resetAllocatorCallBack)(void);
void (*setPerThreadLocalThreadDataCallBack)(struct thread_entry *threadEntry);
#endif //REDIS_FLAT_COMBINING_H
