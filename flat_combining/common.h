//
// Created by se00598 on 15/08/23.
//

#ifndef MANGOSTEEN_INSTRUMENTATION_COMMON_H
#define MANGOSTEEN_INSTRUMENTATION_COMMON_H
//
// Created by se00598 on 5/5/22.
//

#ifndef SM_WITH_STATIC_MEMORY_HEAP_ALLOCATOR_COMMON_H
#define SM_WITH_STATIC_MEMORY_HEAP_ALLOCATOR_COMMON_H
#define CMD_REQUEST_BUFFER_SIZE 20
#define MESSAGE_BUFFER_SIZE 2000

#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>


#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif
//#define DEBUG 0
//#define MANGOSTEEN_DEBUG
pthread_mutex_t h_table_lock;
void *retrieveArguments(char *command_request, int argc);
int retrieveNumberOfArguments(char *command_request);

struct server_response{
    unsigned long nextCommandIndex;
    unsigned long commandLength;
    char command[MESSAGE_BUFFER_SIZE];
};
struct server_event_loop_thread_context{
    int port_number;
    struct ring_buffer *rb;
};
struct metaData{
    char msg[10];
    void *startAddr;
    struct memoryAgnosticRoot *marPtr;
    size_t memoryBlockSize;
    //heap_t *heap;
    void *region;
    //struct application_data app_data;
    unsigned long nextCommandIndex;
};

void sm_op_begin(void);
void sm_op_end(void);
void allocate_on_dram_start(void);
void allocate_on_dram_finish(void);
bool file_exists(const char *path);
#endif //SM_WITH_STATIC_MEMORY_HEAP_ALLOCATOR_COMMON_H
#endif //MANGOSTEEN_INSTRUMENTATION_COMMON_H
