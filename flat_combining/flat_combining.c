//
// Created by se00598 on 10/5/22.
//
#define _GNU_SOURCE
#include "jemalloc/jemalloc.h"
#include "flat_combining.h"
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <emmintrin.h>
#include "dr_annotations.h"

#include <pthread.h>
#include <stdint.h>
#include <errno.h>

#include <dlfcn.h>
#include <stdarg.h>
#include "writer_preference_spin_lock.h"
#include "common.h"
#include "../netutils/netutils.h"
#include "../ring_buffer/ring_buffer.h"

//#define malloc(size) je_malloc(size)
//#define mallocx(size,flags) je_mallocx(size,flags)

pthread_mutex_t threadInitMutex = PTHREAD_MUTEX_INITIALIZER;



extern void mangosteenResetMemoryAllocator(mangosteen_memory_allocator *mma){
    mma->offset = 0;
    //bzero(&mma->threadLocalArena[0], mangosteenAllocatorArenaSize);
}

extern void *mangosteenAllocateMemory(mangosteen_memory_allocator *mma, int size){
    //printf("Allocating %d , offset: %d\n",size, mma->offset);
    void *returnValue = &mma->threadLocalArena[mma->offset];
    mma->offset+=size;
    //printf("New offset: %d\n", mma->offset);
    assert(mma->offset < mangosteenAllocatorArenaSize);
    return returnValue;
}

void take_read_lock(int thread_id) {
    readLock(thread_id);
}

void exec_ro(struct thread_entry *threadEntry) {
#ifdef DEBUG_FLAT_COMBINING
    printf("EXEC_RO: Thread %d\n", threadEntry->thread_number);
#endif
    take_read_lock(threadEntry->thread_number);
#ifdef DEBUG_FLAT_COMBINING
    printf("TOOK READ LOCK: Thread %d\n", threadEntry->thread_number);
#endif
    processRequestCallBack(threadEntry->applicationSpecificStruct);
#ifdef DEBUG_FLAT_COMBINING
    printf("RO Lock acquired for thread %d\n", threadEntry->thread_number);
#endif
    readUnlock(threadEntry->thread_number);
#ifdef DEBUG_FLAT_COMBINING
    printf("RO Lock released by thread %d\n", threadEntry->thread_number);
#endif
}

struct profiling_data {
    uint64_t acc_fc_combiner_ops;
    unsigned long acc_fc_branch_ticks;
    unsigned long acc_fc_proc_ticks;
    unsigned long acc_fc_log_ticks;
    uint64_t acc_rw_ops;
    unsigned long acc_wr_branch_ticks;
    unsigned long acc_wr_proc_ticks;
    unsigned long acc_wr_log_ticks;
    unsigned long number_of_times_there_was_no_more_tasks;
};

void exec_rw_flat_combining(struct thread_entry *threadEntry, struct profiling_data *profilingData) {
    threadEntry->ready = READY_TO_EXECUTE;
    while (true) {
#ifdef DEBUG_FLAT_COMBINING
        printf("Thread %d is looping\n", threadEntry->thread_number);
#endif
        if (threadEntry->ready == STARTED) {
            while (threadEntry->ready != DONE) {
                Pause();
            }
            threadEntry->ready = NONE;
            break;
        }
        if (threadEntry->ready == DONE) {
            threadEntry->ready = NONE;
            break;
        }
        if (lockIsFree()) {
            if (lockWriter()) {
                int number_of_comamnds = 0;
                int ready_commands_indexes[NUMBER_OF_THREADS];

                int defferedMmapsCounter = 0;
                const int defferedMmapsSize = 128;
                ring_buffer_map_entry defferedMmaps[defferedMmapsSize];



                //_mm_sfence();

                while (true) {
                    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
                        if (taskArray[i].ready == READY_TO_EXECUTE) {
                            taskArray[i].ready = STARTED;
                            ready_commands_indexes[number_of_comamnds] = i;
                            number_of_comamnds++;
                            /*
                            int mmaps = taskArray[i].deferredMmapEntries.number_of_regions_in_this_batch;
                            for(int j=0; j < mmaps; j++){
                                memcpy(&defferedMmaps[defferedMmapsCounter], &taskArray[i].deferredMmapEntries.transient_mmap_entries[j], sizeof(ring_buffer_map_entry));
                                defferedMmapsCounter+=1;
                                assert(defferedMmapsCounter < defferedMmapsSize);
                            }
                             */
                        }
                    }

                    if (activeReadersSetIsEmpty()) { // This means we are still waiting for readers to drain
                        break;
                    }
                }
                //_mm_sfence();
#ifdef DEBUG_FLAT_COMBINING
                printf("Starting instrumentation\n");
#endif
                instrument_start();
#ifdef SPLIT_ALLOCATOR
                enableCombinerCallBack();
#endif
                _mm_sfence();
#ifdef DEBUG_FLAT_COMBINING
                printf("Post combiner call back\n");
#endif
/*
                printf("Combiner collected %d mmaps:\n", defferedMmapsCounter);
                for(int i =0; i < defferedMmapsCounter; i++){
                    printf("addr: %p size: %lu\n", defferedMmaps[i].addr, defferedMmaps[i].size);
                }
*/
                // TODO investigate if fences are required... Also move this down after reader set is drained

                volatile int index = 0;
                while (index < number_of_comamnds) {
#ifdef DEBUG_FLAT_COMBINING
                    printf("Processing thread: %d\n", taskArray[ready_commands_indexes[index]].thread_number);
#endif
                    //TODO Prefetch before taking the lock?

                    processRequestCallBack(taskArray[ready_commands_indexes[index]].applicationSpecificStruct);

                    index++;
                }
                _mm_sfence();
                    instrument_stop();
#ifdef SPLIT_ALLOCATOR
                    disableCombinerCallBack();
#endif

                index = 0;
                while (index < number_of_comamnds) {
                    taskArray[ready_commands_indexes[index]].ready = DONE;
                    index++;
                }
                threadEntry->ready = NONE;
                _mm_sfence();
                writeUnlock();
                break;
            }
        }
        Pause();
    }
}

void mangosteen_execute_transactionally(){
    /*
    struct thread_entry *threadEntry = client->threadEntry;
    bool readOnly = isReadOnlyCallBack(client->applicationSpecificStruct);

    threadEntry->applicationSpecificStruct = client->applicationSpecificStruct;

    if(!readOnly) {
        exec_rw_flat_combining(client->threadEntry, client->profilingData);
        volatile bool condition = true;
        if (condition) {
            reset_hash_set();
        }
    }
    else {
        exec_ro(threadEntry);
    }
     */
}

void execute_using_flat_combining_no_rpc(serialized_app_command *serializedAppCommand){
    int *thread_id = pthread_getspecific(per_thread_key);
    struct thread_entry *threadEntry = &taskArray[*thread_id];
    bool readOnly = isReadOnlyCallBack(serializedAppCommand);

    threadEntry->applicationSpecificStruct = serializedAppCommand;

    if(!readOnly) {
/*
        volatile bool condition = true;
        if (condition) {
            reset_hash_set();
        }
*/
        exec_rw_flat_combining(threadEntry, NULL);
/*
        if (condition) {
            reset_hash_set();
        }
        */
    }
    else {
        exec_ro(threadEntry);
    }
}

void clientCmd(serialized_app_command *serializedAppCommand){
	printf("In flat combining\n");
    execute_using_flat_combining_no_rpc(serializedAppCommand);
}

void execute_using_flat_combining(mangosteen_client_t* client){
    struct thread_entry *threadEntry = client->threadEntry;
    //threadEntry->mallctl1("thread.tcache.flush", NULL,NULL, NULL, 0);
    disableCombinerCallBack();
    loadServiceRequestCallBack(client->applicationSpecificStruct, client->req);
    bool readOnly = isReadOnlyCallBack(client->applicationSpecificStruct);

    threadEntry->applicationSpecificStruct = client->applicationSpecificStruct;

    //enableCombinerCallBack();
    //resetAllocatorCallBack();
    //threadEntry->mallctl1("thread.tcache.flush", NULL,NULL, NULL, 0);

    if(!readOnly) {
        disableCombinerCallBack();
        //printf("calling collect_mmaps\n");
        /*
        collect_mmaps(&threadEntry->deferredMmapEntries);
        //printf("post collect_mmaps\n");
        int index = threadEntry->deferredMmapEntries.number_of_regions_in_this_batch;

        while(index > 0){
            index-=1;
            printf("Thread %d collected mmap %p with size: %lu\n",
                   threadEntry->thread_number,
                   threadEntry->deferredMmapEntries.transient_mmap_entries[index].addr,
                   threadEntry->deferredMmapEntries.transient_mmap_entries[index].size);
        }
*/
        exec_rw_flat_combining(client->threadEntry, client->profilingData);
        volatile bool condition = true;
        if (condition) {
            reset_hash_set();
        }
    }
    else {
        disableCombinerCallBack();
        exec_ro(client->threadEntry);
    }
    //resetAllocatorCallBack();
    //enableCombinerCallBack();
}

void prepare_client_response(mangosteen_client_t* client){
    assert(client->applicationSpecificStruct != NULL);
    generic_response genericResponse;
    getResponseInfoCallBack(client->applicationSpecificStruct, &genericResponse);
    client->response_len = genericResponse.size;
    client->left = client->response_len;
    assert(client->left > 0);
    client->responce_buffer = genericResponse.response;
}




void process_command(mangosteen_client_t* client) {
    //printf("in process command\n");
    if(client->applicationSpecificStruct == NULL){
        int fd = 42;

while(1) {
    if (lockIsFree()) {
        //printf("l: 128\n");
        if (lockWriter()) {
            while (!activeReadersSetIsEmpty()) { // This means we are still waiting for readers to drain
                //printf("l: 161\n");
            }
            break;
        }
    }
}
        //usleep(1000);
        //
        //instrument_start();
        //enableCombinerCallBack();
        disableCombinerCallBack();
        client->applicationSpecificStruct = allocateThreadLocalServiceRequestStructCallBack();
        //disableCombinerCallBack();
        ///instrument_stop();
        writeUnlock();
    }
    //printf("Calling exec fc\n");
    execute_using_flat_combining(client);
    //printf("Calling prepare response\n");
    prepare_client_response(client);
}


void do_event(mangosteen_client_t* client) {
    //printf("in do event\n");
    pid_t tid = gettid();
    int connfd = client->connfd;
    char* req = client->req;

    if (client->status == Reading) {
        /*
         We are currently in reading mode.
         Read as many bytes as possible until we get
         a completed request or EAGAIN indicating no
         more data

         If received a complete request,
         process it using Mangosteen and then once ready
         to send a response switch to writing mode.

         Note that at this point any further
         read of the fd should give EAGAIN,
         since we assume no request pipelining.
         */

#if DEBUG
        printf("%d Reading on connfd=%d\n", tid, connfd);
#endif
        bool finished = false;
        while(1) {
            assert(MAX_REQUEST_SIZE-client->bytes_read > 0);
            int r = read(connfd, req + client->bytes_read, MAX_REQUEST_SIZE - client->bytes_read);
            if (r > 0) {
#if DEBUG
                printf("%d Read %d bytes on connfd=%d\n",tid, r, connfd);
#endif
                client->bytes_read += r;
                if (client->bytes_read > MAX_REQUEST_SIZE) {
                    // Too large
                    client->status = Ended;
                    perror("Request too big.");
                    return;
                }

                continue;
            }
            else if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
#if DEBUG
                    printf("%d got EAGAIN reading.\n", tid);
#endif
                    // read end normally
                    //Check for end of request marker (assume '\n' for now)
                    // TODO THIS is APP specific
                    if (client->bytes_read > 2 && client->req[client->bytes_read-2] == '\r' && client->req[client->bytes_read-1] == '\n') {
#if DEBUG
                        printf("%d Finished reading request (connfd=%d).\n", tid, connfd);
#endif
                        finished = true;
                    }
                    break;
                } else {
                    perror("read");
                    break;
                }
            }
            else if (r == 0) {
#if DEBUG
                printf("%d eof (connfd=%d),errno=%d status=Reading.\n",tid, errno, connfd);
#endif
                //perror("eof");
                client->status = Ended;
                return;
            }
        }

        // Two ways to get here: received complete request or conn empty
        if (finished) {
            //For now, just process this request.
            //TODO: Might want to postpone this until after all
            //events processed.
#ifdef FAKE_RESPONSE
            parse_command(client);
#endif
            process_command(client);
            client->bytes_read = 0;
            client->status = Writing;
        }
        return;

    }
    else if (client->status == Writing) {
        //TODO: Write the response.
        //For now just echo the request.
        size_t left = client->left;
        ssize_t written;

#if DEBUG
        printf("%d writing (connfd=%d)\n", tid, connfd);
#endif
        while (left > 0) {

            assert(left > 0);
            written = send(client->connfd, client->responce_buffer + (client->response_len-left), left, 0);
            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // conn blocked
#if DEBUG
                    printf("%d got EAGAIN writing.\n", tid);
#endif
                    break;
                } else {
                    perror("response");
                    client->status = Ended;
                    //resetThreadLocalServiceRequestStruct
                    return;
                }
            } else if (written == 0) {
                // EOF encountered
                perror("response - eof");
                client->status = Ended;
                return;
            } else {
                left -= written;
            }
        }

        if (left == 0) {
#if DEBUG
            printf("%d finished writing (connfd=%d)\n", tid, connfd);
#endif
            client->response_len = 0;
            //free(client->responce_buffer);
            client->responce_buffer = NULL;
            client->status = Reading;
        } else {
            client->left = left;
        }
    }
    else {
        perror("Server logic error.");
        exit(1);
    }

}

void handle_accept_events(struct thread_entry *threadEntry, int listenfd, int epollfd){
#if DEBUG
    printf("%d Received connection notification\n", tid);
#endif

    //could be more than one new connection, hence while loop.
    while (1) {
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_len = sizeof(clnt_addr);
        int connfd = accept(listenfd, (struct sockaddr*)&clnt_addr, &clnt_addr_len);
        if (connfd < 0) {
            if (errno == EAGAIN | errno == EWOULDBLOCK) {
#if DEBUG
                printf("%d got EAGAIN on listen socket\n", tid);
#endif
                break;
            } else {
                perror("accept");
                break;
            }
        }

#if DEBUG
        printf("%d Accepted connection, connfd=%d\n", tid, connfd);
#endif

        setnonblocking(connfd);

        // This will store the connection/client metadata/data.
#ifdef DEBUG_FLAT_COMBINING
        if(threadEntry->is_recovering) {
            printf("before malloc\n");
        }
#endif
        mangosteen_client_t *client = (mangosteen_client_t *)malloc(sizeof(mangosteen_client_t));
        char *req = (char *)malloc(MAX_REQUEST_SIZE);
#ifdef DEBUG_FLAT_COMBINING
        if(threadEntry->is_recovering) {
            printf("after malloc\n");
        }
#endif
        if (client == NULL || req == NULL) {
            perror("malloc");
            exit(1);
        }

#if LOCKING
        pthread_mutex_init(&client->mutex, NULL);
                    assert(pthread_mutex_lock(&(client->mutex)) == 0);
#endif
        client->connfd = connfd;
        client->status = Reading;
        client->req = req;
        client->bytes_read = 0;
        client->responce_buffer = NULL;
        client->response_len = 0;
        client->left = 0;
        pthread_mutex_init(&client->mutex, NULL);
        ///

        client->threadEntry = threadEntry;
        client->applicationSpecificStruct = NULL;
        mangosteenResetMemoryAllocator(&client->mangosteenMemoryAllocator);
        ///
#if LOCKING
        assert(pthread_mutex_unlock(&(client->mutex)) == 0);
#endif
#ifdef DEBUG_FLAT_COMBINING
        if(threadEntry->is_recovering) {
            printf("before malloc l 398\n");
        }
#endif
        struct epoll_event* ev_ptr = (struct epoll_event*)malloc(sizeof(struct epoll_event));
#ifdef DEBUG_FLAT_COMBINING
        if(threadEntry->is_recovering) {
            printf("after malloc l 402\n");
        }
#endif
        //Wait for relevant events on new socket
        //ev_ptr->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        //ev_ptr->data.ptr = client;
        ev_ptr->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        ev_ptr->data.ptr = client;

#if DEBUG
        printf("%d Adding epoll event for connfd=%d\n", tid, connfd);
                    printf("%d ", tid);
                    //print_mask(ev_ptr->events);
                    //print_mask(ev.events);
                    printf("\n");
#endif
        //if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, ev_ptr) < 0) {
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, ev_ptr) < 0) {

            perror("epoll_ctl");
            free(ev_ptr);
            continue;
        } else{
            printf("Thread %d accepted connection\n", threadEntry->thread_number);
            threadEntry->client_connections++;
        }
        free(ev_ptr);

    }
}

void handle_request_event(struct thread_entry *threadEntry, int epollfd, struct epoll_event *epollEvent){
    mangosteen_client_t* client = (mangosteen_client_t *) epollEvent->data.ptr;
#if LOCKING
    assert(pthread_mutex_lock(&(client->mutex)) == 0);
#endif
#if DEBUG
    printf("%d Received data notification on conn=%d\n", tid, client->connfd);
#endif
    if ((epollEvent->events & EPOLLERR) ||
        (epollEvent->events & EPOLLHUP)) {
        //perror("epoll error/hup");
        close (client->connfd);
#if LOCKING
        assert(pthread_mutex_unlock(&(client->mutex)) == 0);
#endif

        printf("thread %d listened to :%d clients\n",threadEntry->thread_number, threadEntry->client_connections);
        return;
    }

#if DEBUG
        printf("%d Status=%d ", tid, client->status);
               // print_mask(events[n].events);
                printf("\n");
#endif

    assert(epollEvent->events != 0);
    if (client->status == Reading) { assert(epollEvent->events & EPOLLIN); }
    else if (client->status == Writing) { assert(epollEvent->events & EPOLLOUT); }
#ifdef DEBUG_FLAT_COMBINING
    printf("Calling do_event\n");
#endif
    ///
    mangosteenResetMemoryAllocator((mangosteen_memory_allocator *) &client->mangosteenMemoryAllocator);
    ///
    //pthread_mutex_lock(&client->mutex);
    do_event(client);
    //pthread_mutex_unlock(&client->mutex);
#ifdef DEBUG_FLAT_COMBINING
    printf("returned from do_event\n");
#endif

    //rearm
    // Presume reading and writing on same connection mutually exclusive!
#ifdef DEBUG_FLAT_COMBINING
    if(threadEntry->is_recovering) {
        printf("before malloc l 476\n");
    }
#endif
    struct epoll_event* ev_ptr = (struct epoll_event*)malloc(sizeof(struct epoll_event));
#ifdef DEBUG_FLAT_COMBINING
    if(threadEntry->is_recovering) {
        printf("after malloc l 480\n");
    }
#endif
    if (client->status == Reading) {
        ev_ptr->events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    }
    else if (client->status == Writing) {
        ev_ptr->events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
    }
    else {
        if (client->status != Ended) {
            perror("Logic error");
            exit(1);
        }

        int connfd = client->connfd;
        close(client->connfd);
        free(client->req);
        free(ev_ptr);

#if LOCKING
        assert(pthread_mutex_unlock(&(client->mutex)) == 0);
#endif
        threadEntry->transientFree(client->applicationSpecificStruct);
        free(client);
        // TODO free app specific struct
        // disable combiner
        // call app specific free?
        return;
    }

#if DEBUG
    printf("%d Calling epoll_ctl on conn=%d, events=%d\n", tid, client->connfd, ev_ptr->events);
                printf("%d ", tid);
                //print_mask(ev_ptr->events);
                printf("\n");
#endif
    ev_ptr->data.ptr = client;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, client->connfd, ev_ptr) < 0) {
        perror("epoll_ctl");
#if LOCKING
        assert(pthread_mutex_unlock(&(client->mutex)) == 0);
#endif
        return;
    }
    free(ev_ptr);
#if LOCKING
    assert(pthread_mutex_unlock(&(client->mutex)) == 0);
#endif
}

void handle_event(struct epoll_event *epollEvent, int listenfd, int epollfd, struct thread_entry *threadEntry){
    // If new connections
#ifdef DEBUG_FLAT_COMBINING
    //printf("%d Looping on events iteration %d\n", tid, n);
#endif
    if (epollEvent->data.fd == listenfd) {
        handle_accept_events(threadEntry, listenfd, epollfd);
    } else {
        handle_request_event(threadEntry, epollfd, epollEvent);
    }
}

int setup_listen_fd(){
    int listenfd = open_listenfd(PORT_NUMBER);
    setnonblocking(listenfd);
    return listenfd;
}

int set_up_epoll(int listenfd){
    int epollfd = epoll_create1(0);
    if (epollfd < 0) {
        fprintf(stderr, "error while creating epoll fd\n");
        exit(1);
    }
    struct epoll_event *epevent = malloc(sizeof (struct epoll_event));
    epevent->events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    epevent->data.fd = listenfd;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, epevent) < 0) {
        fprintf(stderr, "error while adding listen fd to epoll inst\n");
        exit(1);
    }
    return epollfd;
}

void start_event_loop(struct thread_entry *threadEntry){
    printf("about to allocate memory\n");
    struct epoll_event *events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*MAX_EVENTS);
    printf("Memory allocated\n");
    assert(events != NULL);
    if (events == NULL) {
        perror("malloc");
    }

    int listenfd = setup_listen_fd();
    int epollfd = set_up_epoll(listenfd);

    int nfds;
#ifdef DEBUG_FLAT_COMBINING
    printf("Thread Started\n");
#endif


    while(1) {
        // Wait on epoll
        //nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
#ifdef DEBUG_FLAT_COMBINING
        printf("Waiting for epoll\n");
#endif
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
#ifdef DEBUG_FLAT_COMBINING
        printf("Thread woken up\n");
#endif
        if (nfds <= 0) {
            perror("epoll_wait");
            continue;
        }
        for (int n = 0; n < nfds; ++n) {
            handle_event(&events[n], listenfd, epollfd, threadEntry);
        }
    }
    free(events);
}

void configure_cpu_set(struct thread_entry *threadEntry){
#ifndef RUN_WITHOUT_CPU_SET
    int cpu_offset =1;
    pthread_t thread = pthread_self();
    if(!threadEntry->is_recovering) {
        cpu_set_t cpuSet;
        //TODO FIND OUT HOW TO LINK THESE

        CPU_ZERO(&cpuSet);
        CPU_SET(threadEntry->thread_number + cpu_offset, &cpuSet);
        int res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuSet);

        if (res != 0) {
            printf("ERROR set afinity! ID: %d\n", threadEntry->thread_number);
            exit(1);
        }

        res = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuSet);

        if (res != 0) {
            printf("ERROR get afinity!\n");
            exit(1);
        }
    }
#endif
}

_Noreturn void *start_worker_thread(void *args) {
    pthread_mutex_lock(&threadInitMutex);
    initializedThreads+=1;
    pthread_mutex_unlock(&threadInitMutex);

    struct thread_entry *threadEntry = args;
#ifdef DEBUG_FLAT_COMBINING
    printf("Waiting Thread Recovery \n");
#endif
    while(threadEntry->ready != READY_TO_INTIALISE){
        sleep(1);
    }

    configure_cpu_set(threadEntry);
/*
    void* (*transientAlloc)(size_t);
    transientAlloc = malloc;
    threadEntry->transientAlloc = transientAlloc;
    */
    threadEntry->transientAlloc = (void* (*)(size_t))dlsym(threadEntry->handle, "malloc");
    threadEntry->transientFree = (void (*)(void*))dlsym(threadEntry->handle, "free");
    int (*mallctl1)(char*, void*,void*, void*, size_t) = dlsym(threadEntry->handle, "mallctl");
    assert(mallctl!=NULL);
    void (*mallctl2)(char*, void*,void*, void*, size_t) = dlsym(threadEntry->handle, "je_mallctl");
    threadEntry->mallctl1 = mallctl2;
    bool oldv;
    bool newval = false;
    size_t sizeofboll = sizeof (bool);
    size_t *psize;
/*
    mallctl1("tcache.create", NULL, NULL, NULL, 0);
    mallctl1("thread.arena", NULL, NULL, NULL, 0);
    mallctl1("thread.tcache.flush", NULL,NULL, NULL, 0);
            printf("Old val: %d\n", oldv);
*/

    /*
     void* handle1 = dlmopen(LM_ID_NEWLM, "/home/se00598/home/se00598/persimmon/redis/deps/jemalloc/lib/libjemalloc.a", RTLD_NOW);

     threadEntry->transientAlloc = (void* (*)(size_t))dlsym(handle1, "malloc");
     void (*free3)(void*) = (void (*)(void*))dlsym(handle1, "free");
     void (*malloc_stats3)(void) = (void (*)(void))dlsym(handle1, "malloc_stats");
     //threadEntry->transientAlloc = malloc3;
      */
/*
    for (int i = 0; i < 20; i++) {
        //void* p3 = (*malloc3)(512);
        void* p3 = (threadEntry->transientAlloc)(512);
        (*malloc_stats3)();
        free3(p3);
    }
*/
    printf("Working properly\n");

    setPerThreadLocalThreadDataCallBack(threadEntry);
    start_event_loop(threadEntry);
}

void allocateFlatCombiningdMemory(){
    int padding_size = 128/sizeof(int);
    int number_of_units = NUMBER_OF_THREADS * padding_size;
    readers_set = (int*) aligned_alloc(128, number_of_units*sizeof (int));

    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        readers_set[(i*padding_size)] = NOT_READING;
    }
    int alignment = 128;
    taskArray = aligned_alloc( alignment, sizeof (struct thread_entry)*NUMBER_OF_THREADS);
}


