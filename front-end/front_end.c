//
// Created by se00598 on 23/02/24.
//
#define _GNU_SOURCE

#include <stdbool.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include "front_end.h"
#include "../instrumentation/instrument_front_end.h"
#include "../flat_combining/flat_combining.h"
#include "../region_table/region_table.h"
#include "../netutils/netutils.h"

int global_thread_count = 0;


__thread int thread_index;
void mangosteen_initialize_thread(){
    printf("About to take a mutex \n");
    // TODO add proper initialization
    pthread_mutex_lock(&mutex);
    int tid = gettid();
    printf("Thread id: %d\n", tid);
    //int *thread_index = malloc(sizeof(int));
    thread_index = global_thread_count;
    //pthread_setspecific(per_thread_key, &thread_index);
    //int *thread_index = pthread_getspecific(per_thread_key);
    taskArray[thread_index].ready = NONE;
    taskArray[thread_index].connection_slot = CONNECTION_AVAILABLE;
    taskArray[thread_index].thread_number = thread_index;
    // TODO add transient allocator handle
    //taskArray[*thread_index].handle = transient_allocator_handle;
    explicit_bzero(&taskArray[thread_index].command_request, 2048);

    global_thread_count++;

    configure_cpu_set(&taskArray[thread_index]);
    pthread_mutex_unlock(&mutex);
    
    while (global_thread_count < NUMBER_OF_THREADS-1)

    {
        printf("waiting for others\n");
        
    }
    
    
}

void start_worker_threads(pthread_t *threadArray,  struct thread_args *threadArgs, void *transient_allocator_handle) {
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("Created thread: %d address %lu\n", i, &taskArray[i]);
#endif
        taskArray[i].connection_slot = CONNECTION_AVAILABLE;
        taskArray[i].thread_number = i;
        taskArray[i].listenfd = threadArgs->listenfd;
        taskArray[i].epollfd = threadArgs->epollfd;
        taskArray[i].handle = transient_allocator_handle;
        explicit_bzero(&taskArray[i].command_request, 2048);
        pthread_cond_init(&taskArray[i].cv, NULL);
        pthread_cond_init(&taskArray[i].rv, NULL);
        pthread_mutex_init(&taskArray[i].cp, NULL);
        pthread_mutex_init(&taskArray[i].rp, NULL);
        __sync_lock_test_and_set(&taskArray[i].ready, NONE);
        __sync_synchronize();
        pthread_create(&threadArray[i], NULL, start_worker_thread, &taskArray[i]);
    }
}

void restore_memory() {
#ifdef DEBUG_MANGOSTEEN_MAIN
    printf("copy_data_from_pmem start\n");
#endif
    region_table *regionTable = instrument_args.region_table;
    uint64_t number_of_entries = regionTable->number_of_entries;
    for (uint64_t i = 0; i < number_of_entries; i++) {
        char fileName[MAX_FILE_NAME_SIZE];
        get_file_name_for_sequence_number(fileName, i);
        int fd = open(fileName, O_RDWR, 0666);
        if (fd == -1) {
            printf("ERROR OPENING FILE\n");
        }
        region_table_entry *regionTableEntry = &regionTable->regionTable[i];
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("Opening file: %s which corresponds to region %p with size: %lu\n",fileName, regionTableEntry->address, regionTableEntry->mmap_size);
#endif
        void *source = mmap(NULL, regionTableEntry->mmap_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("source map is ok\n");
#endif
        void *destination = regionTableEntry->address;
        void *returnedMmapAddress = mmap(destination, regionTableEntry->mmap_size, PROT_READ | PROT_WRITE,
                                         MAP_FIXED | MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
        assert(returnedMmapAddress == destination);
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("destination map is ok: %p\n", returnedMmapAddress);
#endif
        memcpy(destination, source, regionTableEntry->mmap_size);
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("memcpy is okay\n");
#endif
        munmap(source, regionTableEntry->mmap_size);
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("munmap is okay\n");
#endif
    }
#ifdef DEBUG_MANGOSTEEN_MAIN
    printf("copy_data_from_pmem done\n");
#endif
}

void start_front_end(bool rpc_mode, bool newProcess) {
    printf("Launching front end\n");
    const char *sem_file_name = "init_semaphore";
    sem_t *sem = sem_open(sem_file_name, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pthread_t threadArray[NUMBER_OF_THREADS];
    void *transient_allocator_handle = dlmopen(LM_ID_NEWLM, "lib/libjemalloc.so", RTLD_NOW);
    struct thread_args targs;
    if (!newProcess) {
        printf("Starting recovery\n");
        printf("Waiting for backend semaphore\n");
        sem_wait(sem);
        sem_close(sem);

        printf("Starting memory restoration\n");
        restore_memory();
        printf("Memory is restored\n");

        // Might need to do more resets here...
        reset_mmaped_regions_counter();
        instrument_args.state = INSTRUMENTATION_DEFAULT_STATE;
#ifndef DISABLE_INSTRUMENTATION_AFTER_RECOVERY
        mangosteen_instrument_init_front_end();
#endif
        if(rpc_mode) {
            start_worker_threads(threadArray, &targs, transient_allocator_handle);
        }
#ifndef EPOLL_SERVER
        start_server_thread();
#endif
        if(rpc_mode) {
            for (int i = 0; i < NUMBER_OF_THREADS; i++) {
                __sync_lock_test_and_set(&taskArray[i].is_recovering, true);
            }
        }
        printf("Recovery complete\n");
    } else {
        instrument_args.state = INSTRUMENTATION_DEFAULT_STATE;
#ifndef RUN_WITHOUT_INSTRUMENTATION
        mangosteen_instrument_init_front_end();
#endif


        if(rpc_mode){
            start_worker_threads(threadArray, &targs, transient_allocator_handle);
        }
        sem_post(sem);
        //sem_wait(sem);
        sem_close(sem);
        if(rpc_mode) {
            for (int i = 0; i < NUMBER_OF_THREADS; i++) {
                __sync_lock_test_and_set(&taskArray[i].is_recovering, false);
            }
        }
    };
    if(rpc_mode) {
        for (int i = 0; i < NUMBER_OF_THREADS; i++) {
            __sync_lock_test_and_set(&taskArray[i].ready, READY_TO_INTIALISE);
        }
        for (int i = 0; i < NUMBER_OF_THREADS; i++) {
#ifdef DEBUG_MANGOSTEEN_MAIN
            printf("joining: %llu, index: %d\n", (unsigned long long) threadArray[i], i);
#endif
            pthread_join(threadArray[i], NULL);
        }
    }
}
