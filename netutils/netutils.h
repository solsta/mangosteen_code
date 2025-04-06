//
// Created by se00598 on 10/10/23.
//

#ifndef MANGOSTEEN_INSTRUMENTATION_NETUTILS_H
#define MANGOSTEEN_INSTRUMENTATION_NETUTILS_H
#include <limits.h>
#include <stddef.h>
#include <stdint-gcc.h>
#include <sys/epoll.h>
//#include "../mangosteen_instrumentation.h"
//#include <../mangosteen_instrumentation.h>

#define BUF_SIZE 10485760
#define THREAD_NUM 4
#define MAX_EVENTS 100
#define MAX_REQUEST_SIZE 2048
#define MAX_CONN 256


struct thread_args {
    int listenfd;
    int epollfd;
};

typedef enum req_status {
    Reading,
    Writing,
    Actively_reading,
    Ended
} req_status_t;

typedef enum cmd_type {
    HMSET,
    ZADD
} cmd_type_t;

typedef struct mangosteen_client {
    int connfd;
    req_status_t status;
    char* req;
    size_t bytes_read;  //How many bytes so far
    size_t left; // How many to write
    size_t responce_size;
    char *responce_buffer;
    void *applicationSpecificStruct;
    cmd_type_t cmd;
    size_t response_len;
    char mangosteenMemoryAllocator[2004];
    void *threadEntry;
    void *profilingData;
    pthread_mutex_t mutex;
    //mangosteen_memory_allocator mangosteenMemoryAllocator;
} mangosteen_client_t;

int open_listenfd(uint16_t port);
void setnonblocking(int fd);
#endif //MANGOSTEEN_INSTRUMENTATION_NETUTILS_H
