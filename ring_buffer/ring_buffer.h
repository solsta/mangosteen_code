//
// Created by se00598 on 10/07/23.
//

#ifndef FIXED_SIZE_RING_BUFFER_RING_BUFFER_H
#define FIXED_SIZE_RING_BUFFER_RING_BUFFER_H
#include <stdio.h>
#include <stdint-gcc.h>
#include <libpmem.h>
//#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "stdbool.h"
#include "../configuration.h"
//#define DEBUG_PRINT
//#define TEST_WITHOUT_BACKEND
#define RING_BUFFER_SIZE 65536
//#define RING_BUFFER_SIZE 4194304


#define RING_BUFFER_FILE_PATH "/home/vagrant/mangosteen_pmem_data/test_outputs/redo_log"
#define MEMORY_DIRECTORY_FILE_PATH "/home/vagrant/mangosteen_pmem_data/test_outputs/maps/"
#define MEMORY_METADATA_START_TAG "S_MEMORY"
#define MEMORY_METADATA_END_TAG "E_MEMORY"
#define REDO_LOG_END_TAG "E_REDOLO"

#define REDO_ENTRY 0
#define MMAP_ENTRY 1
#define COMMIT_ENTRY 2
#define TRANSIENT_MMAP_ENTRY 3

__attribute__((aligned(64))) static const uint64_t ring_buffer_size = RING_BUFFER_SIZE;
__attribute__((aligned(64))) uint64_t global_local_head;
__attribute__((aligned(64))) uint64_t global_local_tail;

typedef struct{
    uint32_t numberOfRegions;
    uint32_t ringBufferHead;
}atomic_pmem_commit;

typedef struct{
    uint8_t entry_type;
    void *addr;
    char payload[PAYLOAD_SIZE];
}ring_buffer_entry;
/*
typedef struct{
    char first_part[8];
    uint64_t size;
}ring_buffer_custom_entry;
*/
#define MAX_TRANSIENT_REGIONS 32

typedef struct{
    uint8_t entry_type;
    void *addr;
    uint64_t size;
}ring_buffer_map_entry;

typedef struct{
    int number_of_regions_in_this_batch;
    ring_buffer_map_entry transient_mmap_entries[MAX_TRANSIENT_REGIONS];
}deferred_mmap_entries;

typedef struct{
    ring_buffer_entry buffer[RING_BUFFER_SIZE];
    _Atomic uint64_t last_persisted_head;
    _Atomic uint64_t last_persisted_tail;
    _Atomic uint64_t remote_head;
    char padding[128];
    _Atomic uint64_t remote_tail;
    char padding2[128];
    void *producer_addr;
    atomic_pmem_commit commit_metadata;
    char debug_string[10];
}ring_buffer;

ring_buffer  *ring_buffer_create_and_initialise(ring_buffer *ringBuffer);
ring_buffer *ring_buffer_open(bool init);
void ring_buffer_enqueue_redo_log_entry(ring_buffer *ringBuffer, void *addr, uint64_t *thread_local_head,uint64_t *thread_local_tail);
ring_buffer *map_ring_buffer_file_to_correct_address();
ring_buffer_entry *ring_buffer_dequeue(ring_buffer *ringBuffer);
void ring_buffer_commit_enqueued_entries(ring_buffer *ringBuffer,uint64_t *thread_local_head,uint64_t *thread_local_tail);
void ring_buffer_enqueue_mmap_entry(ring_buffer *ringBuffer, ring_buffer_map_entry *ringBufferMapEntry,uint64_t *thread_local_head,uint64_t *thread_local_tail);
void ring_buffer_commit_dequeued_entries(ring_buffer *ringBuffer);
bool has_more_redo_entries(char *msg);
bool has_more_map_entries(char *msg);
bool is_a_memory_tag(char *msg);
void close_ring_buffer(ring_buffer *ringBuffer);
void ring_buffer_print_info(ring_buffer *ringBuffer);
uint64_t get_global_head_value(ring_buffer *ringBuffer);
uint64_t get_global_tail_value(ring_buffer *ringBuffer);
void set_global_head_value(uint64_t thread_local_head,ring_buffer *ringBuffer);
void set_global_tail_value(uint64_t thread_local_tail,ring_buffer *ringBuffer);
void reset_mmaped_regions_counter();
#endif //FIXED_SIZE_RING_BUFFER_RING_BUFFER_H
