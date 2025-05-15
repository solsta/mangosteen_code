//
// Created by se00598 on 26/02/24.
//

#include "instrument_front_end.h"
#include "../ring_buffer/ring_buffer.h"
#include "../region_table/region_table.h"
#include "../configuration.h"


#ifndef MANGOSTEEN_INSTRUMENTATION_INSTRUMENATION_COMMON_H
#define MANGOSTEEN_INSTRUMENTATION_INSTRUMENATION_COMMON_H

#if HASH_SET_SIZE == 65536
#define REAL_HASH_SET_SIZE 524288
#endif

#if HASH_SET_SIZE == 32768
#define REAL_HASH_SET_SIZE 262144
#endif

#if HASH_SET_SIZE == 16384
#define REAL_HASH_SET_SIZE 131072
#endif

#if HASH_SET_SIZE == 8192
#define REAL_HASH_SET_SIZE 65536
#endif

#if HASH_SET_SIZE == 4096
#define REAL_HASH_SET_SIZE 32768
#endif

#if HASH_SET_SIZE == 2048
#define REAL_HASH_SET_SIZE 16382
#endif

#if HASH_SET_SIZE == 1024
#define REAL_HASH_SET_SIZE 8192
#endif

#if HASH_SET_SIZE == 8
#define REAL_HASH_SET_SIZE 64
#endif

#if HASH_SET_SIZE == 0
#define REAL_HASH_SET_SIZE 0
#endif

#if PAYLOAD_SIZE == 32
#define MASK_SHIFT 5
#endif


#define MAX_MMAP_REGIONS 100
static int tls_index;

typedef struct overflow_buffer{
    int size;
    void* buffer[4096];
}overflow_buffer;

overflow_buffer overflowBuffer;

typedef struct hash_set_bypass_entry{
    void *addr;
    long size;
}hash_set_bypass_entry;

typedef struct hash_set_bypass_buffer{
    hash_set_bypass_entry entry_buffer[4096];
}hash_set_bypass_buffer;

hash_set_bypass_buffer hashSetBypassBuffer;

typedef struct assembly_args{
    void *drcontext;
    reg_id_t reg_storing_destination;
    reg_id_t reg_storing_hash_set_pointer;
    reg_id_t range;
    reg_id_t rax;
    reg_id_t value_at_index;
    reg_id_t rsp;
    reg_id_t counter_pointer_reg;
    instr_t *instr;
    instr_t *where;
    instrlist_t *ilist;
    instr_t *memref_instr;
    int pos;
    instr_t *restore_registers;
    uint write_size;
    instr_t *use_overflow_buffer;
    opnd_t ref;

}assembly_args;

typedef struct {
    int size;
    int maxElementsBeforeResize;
    int numberOfEntries; //Each entry is a PAYLOAD_SIZE byte aligned region of memory
    int numberOfCollisions; // All hash collisions
    int numberOfDuplicates; // Collisions caused by duplicate setOfEntries. Subset of [numberOfCollisions].
    // ^ we don't know which ones are duplicates and which ones fit into aligned chunk of memory;
    // Can potentially count hits for each entry, but will also need to keep track of their sizes.
    int numberOfResizes;
} address_hash_set_metadata;

typedef struct {
    char *alignedAddress;
} address_hash_set_entry;

typedef struct {
    int sysnum;
    app_pc addr;
    uint64_t size;
} last_syscall;

#define MAX_NUM_OF_MEMCPY 100

typedef struct memcpy_entry{
    char* start;
    char* end;
}memcpy_entry;

typedef struct memcpy_array{
    int count;
    memcpy_entry array[MAX_NUM_OF_MEMCPY];
}memcpy_array;

// TODO make sure hash_set_entries are aligned?
typedef struct {
    int *number_of_hash_set_entries;
    address_hash_set_entry *hash_set_entries;
    address_hash_set_metadata hash_set_metadata;
    ring_buffer_entry memory_regions_mmaped_in_this_batch[MAX_MMAP_REGIONS];
    last_syscall lastSyscall;
    ring_buffer *pmem_buffer;
    void *cache;
    file_t log;
    char *padding[128];
    int combiner;
    char *padding2[128];
    uint64 num_refs;

    uint64_t thread_local_head;
    uint64_t thread_local_tail;
    int skip_mmap;
    deferred_mmap_entries deferredMmapEntries;
    hash_set_bypass_buffer *hashSetBypassBuffer;
    memcpy_array memcpyArray;
    int threadId;
} per_thread_t;

static void collect_payload_and_store_to_pmem(per_thread_t *data, bool commit)
{
    /** Thread local values are used here to avoid cache invalidation */

    //printf("Starting iteration\n");

#ifdef USE_DEDUPLICATION_BUFFER

    //printf("Entries in overflow buffer %d\n", overflowBuffer.size);
    for(int i=0; i<overflowBuffer.size; i++){
        //printf("Overflow buffer entry: %d : %p payload : %s\n", i, overflowBuffer.buffer[i], overflowBuffer.buffer[i]);
        ring_buffer_enqueue_redo_log_entry(data->pmem_buffer,
                                           overflowBuffer.buffer[i], &data->thread_local_head,
                                           &data->thread_local_tail);
    }

    for (int i = 0; i < HASH_SET_SIZE; i++) {
        if (data->hash_set_entries[i].alignedAddress != 0) {
#ifdef LOG_TO_RING_BUFFER
            //printf("logging: %p, addr %s\n", data->hash_set_entries[i].alignedAddress, data->hash_set_entries[i].alignedAddress);
            //printf("logging: %p ; payload : %s\n", data->hash_set_entries[i].alignedAddress, data->hash_set_entries[i].alignedAddress);
            if(data->hash_set_entries[i].alignedAddress > 1000) {

                ring_buffer_enqueue_redo_log_entry(data->pmem_buffer,
                                                   data->hash_set_entries[i].alignedAddress, &data->thread_local_head,
                                                   &data->thread_local_tail);
            }

#endif
            // TODO get rid of redundant zeroing of hash set in FC
            data->hash_set_entries[i].alignedAddress = 0;
        }
    }
#else
    printf("Number of entries: %d\n", *data->number_of_hash_set_entries);
    for (int i = 0; i < *data->number_of_hash_set_entries; i++) {
        printf("entry: %d addr: %p size: %ld\n", i, data->hashSetBypassBuffer->entry_buffer[i].addr,
               data->hashSetBypassBuffer->entry_buffer[i].size);
        char *addr = data->hashSetBypassBuffer->entry_buffer[i].addr;
        long size = data->hashSetBypassBuffer->entry_buffer[i].size;
        char *entry_boundary = addr + size;
        while (addr < entry_boundary) {
            ring_buffer_enqueue_redo_log_entry(data->pmem_buffer,
                                               addr, &data->thread_local_head,
                                               &data->thread_local_tail);
            addr += PAYLOAD_SIZE;
        }
    }
#endif
#ifdef DEBUG_INSTRUMENTATION
    printf("Finished iteration\n");
#endif
#ifdef LOG_TO_RING_BUFFER
    if(commit) {
        ring_buffer_commit_enqueued_entries(data->pmem_buffer, &data->thread_local_head, &data->thread_local_tail);
    }
#ifdef DEBUG_INSTRUMENTATION
    printf("Head: %lu tail: %lu\n",data->thread_local_head, data->thread_local_tail);
    printf("Committed\n");
#endif
        set_global_head_value(data->thread_local_head,data->pmem_buffer );
        set_global_tail_value(data->thread_local_tail, data->pmem_buffer);

#endif

#ifdef TEST_WITHOUT_BACKEND
    set_global_head_value(0,data->pmem_buffer);
    set_global_tail_value(0, data->pmem_buffer);
#endif

}

#endif //MANGOSTEEN_INSTRUMENTATION_INSTRUMENATION_COMMON_H
