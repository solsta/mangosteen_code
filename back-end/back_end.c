//
// Created by se00598 on 22/02/24.
//
#define _GNU_SOURCE

#include <sys/mman.h>
#include <stdlib.h>
#include <immintrin.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include "back_end.h"
#include "../instrumentation/instrument_front_end.h"


#define ATOMIC_PMEM_SIZE 8

void commit_batch(ring_buffer *ringBuffer) {
    _mm_sfence();
    ring_buffer_commit_dequeued_entries(ringBuffer);
}

void commit_rb_batch(ring_buffer *ringBuffer, region_table *regionTable) {
    atomic_pmem_commit scratch_commit;
    scratch_commit.numberOfRegions = regionTable->uncommited_number_of_entries;
    scratch_commit.ringBufferHead = global_local_head;

    // This is 8 byte struct that is committed atomically.
    pmem_memcpy_persist(&ringBuffer->commit_metadata, &scratch_commit, ATOMIC_PMEM_SIZE);

    region_table_commit(regionTable);
    commit_batch(ringBuffer);
}
long mmapped = 0;
long unmapped =0;
bool addrees_is_mmaped(char *addr, char *payload, region_table *regionTable){
    for(uint64_t i=0; i < regionTable->number_of_entries; i++){
        char *start = regionTable->regionTable[i].address;
        char *finish= regionTable->regionTable[i].address + regionTable->regionTable[i].mmap_size;
        if(addr >= start && addr <= finish){
            mmapped++;
            return true;
        }
    }
    printf("unmapped addr: %p payload: %s\n", addr, payload);
    unmapped++;
    return false;
}

void process_entry(ring_buffer *ringBuffer, region_table *regionTable) {
    ring_buffer_entry *entry = ring_buffer_dequeue(ringBuffer);
//printf("backend processing entry\n");
#ifdef DEBUG_MANGOSTEEN_MAIN
    char *str = entry;
    printf("Consumer looking at: %s\n", str);
#endif
    if (entry->entry_type == MMAP_ENTRY) {

#ifdef USE_PER_BATCH_MEMORY_MMAPING_SYNCRONIZATION
      if(!addrees_is_mmaped(entry->addr, "foobar", regionTable)) {
#endif
#ifndef CONSUMER_SKIP_MMAP

          ring_buffer_map_entry *ringBufferMapEntry = (ring_buffer_map_entry *) entry;
#ifdef DEBUG_MANGOSTEEN_MAIN
          printf("Consumer mmaping entry addr %p, size: %lu\n", ringBufferMapEntry->addr, ringBufferMapEntry->size);
#endif
          printf("Consumer mmaping entry addr %p, size: %lu\n", ringBufferMapEntry->addr, ringBufferMapEntry->size);
          region_table_create_memory_region(regionTable, ringBufferMapEntry->addr, ringBufferMapEntry->size);
#ifdef DEBUG_MANGOSTEEN_MAIN
          printf("MMAP Done\n");
#endif
#ifdef USE_PER_BATCH_MEMORY_MMAPING_SYNCRONIZATION
      }else{
          printf("address was already mmaped\n");
      }
#endif
#endif
    } else if (entry->entry_type == TRANSIENT_MMAP_ENTRY) {
        ring_buffer_map_entry *ringBufferMapEntry = (ring_buffer_map_entry *) entry;
        void *returnedMmapAddress = mmap(ringBufferMapEntry->addr, ringBufferMapEntry->size, PROT_WRITE | PROT_READ,
                                         MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        printf("Consumer mmaping transient entry addr %p, size: %lu\n", ringBufferMapEntry->addr, ringBufferMapEntry->size);
        assert(returnedMmapAddress == ringBufferMapEntry->addr);
    } else if (entry->entry_type == REDO_ENTRY) {
#ifdef USE_PER_BATCH_MEMORY_MMAPING_SYNCRONIZATION
        regionTable->number_of_new_entries = 0;
        if(!addrees_is_mmaped(entry->addr, entry->payload, regionTable)){
            regionTable->needsSync = true;
            printf("waiting to sync mmaps\n");
            while(true){
                if(!regionTable->needsSync){
                    break;
                }

            }
            printf("received ready signal on backend\n");
            for(uint64_t i = 0; i < regionTable->number_of_new_entries; i++){
                printf("mmaping %p size %lu\n", regionTable->newEntries[i].address, regionTable->newEntries[i].mmap_size);
                region_table_create_memory_region(regionTable, regionTable->newEntries[i].address, regionTable->newEntries[i].mmap_size);
                region_table_commit(regionTable);
            }
            regionTable->number_of_new_entries = 0;

         }
#endif
#ifndef CONSUMER_SKIP_REDO
            memcpy(entry->addr, entry->payload, PAYLOAD_SIZE);
            //pmem_persist(entry->addr, PAYLOAD_SIZE);
            pmem_flush(entry->addr, PAYLOAD_SIZE);
#endif
        //}
        //printf("Consumer redo entry addr %p\n", entry->addr);


    } else if (entry->entry_type == COMMIT_ENTRY) {
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("Consumer commits\n");
#endif

        commit_rb_batch(ringBuffer, regionTable);
        __asm__ volatile("mfence" ::: "memory");

#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("mmapped: %lu unmapped %lu\n",mmapped, unmapped);
        printf("Consumed commited\n");
#endif
    } else {
        printf("ERROR incorrect entry type");
        exit(1);
    }
}

_Noreturn void run_consumer(ring_buffer *ringBuffer, region_table *regionTable) {

    while (true) {
        uint64_t tmp = ringBuffer->last_persisted_head;
        while (ringBuffer->last_persisted_tail == tmp) {
            Pause();
            tmp = ringBuffer->last_persisted_head;
        }
        process_entry(ringBuffer, regionTable);
    }
}

void remap_backend(region_table *regionTable) {
#ifdef DEBUG_MANGOSTEEN_MAIN
    printf("Table content:\n");
    for(uint64_t i = 0; i < regionTable->number_of_entries; i++){
        region_table_entry *regionTableEntry = &regionTable->regionTable[i];
        if(regionTableEntry->mmap_size == 2621472){
            dr_fprintf(STDERR,"ERROR tried to remap ring buffer\n");
            exit(1);
        }
        printf("Entry 1: addr: %p, size: %lu, file_index: %lu\n", regionTableEntry->address, regionTableEntry->mmap_size, regionTableEntry->file_index);
    }
#endif
    for (uint64_t i = 0; i < regionTable->number_of_entries; i++) {
        if (true) {
            int prot = PROT_READ | PROT_WRITE;
            int flags = MAP_FIXED | MAP_SHARED_VALIDATE | MAP_SYNC;
            char fileName[MAX_FILE_NAME_SIZE];
            get_file_name_for_sequence_number(fileName, i);
            int fd = open(fileName, O_RDWR, 0666);
            if (fd == -1) {
                printf("ERROR OPENING FILE\n");
            }
            region_table_entry *regionTableEntry = &regionTable->regionTable[i];
#ifdef DEBUG_MANGOSTEEN_MAIN
            printf("Opening file: %s which corresponds to region %p with size: %lu\n", fileName,
                   regionTableEntry->address, regionTableEntry->mmap_size);
#endif
            void *returnedMmapAddress = mmap(regionTableEntry->address, regionTableEntry->mmap_size, prot,
                                             flags, fd, 0);
#ifdef DEBUG_MANGOSTEEN_MAIN
            printf("Returned address: %p\n", returnedMmapAddress);
#endif
            assert(returnedMmapAddress == regionTableEntry->address);
        }
    }

}

void start_back_end(bool isNewProcess) {
    printf("starting backend\n");
    region_table *regionTable = instrument_args.region_table;
    ring_buffer *ringBuffer = instrument_args.ringBuffer;

    printf("opened rt and rb\n");
    const char *sem_file_name = "init_semaphore";
    sem_t *sem = sem_open(sem_file_name, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
    printf("initialized semaphore\n");

    if (!isNewProcess) {
        instrument_args.state = BACK_END_RECOVERY;
        printf("Backend started remaping memory\n");
        remap_backend(regionTable);
        printf("Backend finished remaping memory\n");
        regionTable->number_of_entries = ringBuffer->commit_metadata.numberOfRegions;
        pmem_persist(&regionTable->number_of_entries, 8);

        int i = 0;
        printf("Backend started replaying log\n");
        while (ringBuffer->remote_head != ringBuffer->remote_tail) { // Empty; Wait until new entry appears.
#ifdef DEBUG_MANGOSTEEN_MAIN
            printf("RB: head %lu, tail %lu\n",ringBuffer->remote_head,ringBuffer->remote_tail);
#endif
            printf("HEAD: %lu TAIL: %lu\n", ringBuffer->remote_head, ringBuffer->remote_tail);
            // Consumer breaks once recovered ...
            process_entry(ringBuffer, regionTable);
            printf("Processed entry: %d\n", i);
        }
        printf("Backend finished replaying log\n");
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("Log processed\n");
#endif
        // Wakes up front-end
        sem_post(sem);
        sem_close(sem);
#ifdef DEBUG_MANGOSTEEN_MAIN
        printf("Restoring complete, starting consumer\n");
        printf("Tail: %lu Head: %lu\n", ringBuffer->remote_tail, ringBuffer->remote_head);
#endif
        printf("Sent wake up signal to front-end\n");
#ifndef DISABLE_CONSUMER_AFTER_RECOVERY
/*
        sem_wait(sem);
        sem_close(sem);
        sem_unlink(sem_file_name);
*/
        run_consumer(ringBuffer, regionTable);
#endif
    } else {

        //Pin to core 31 ?
        pthread_t thread = pthread_self();
        sem_wait(sem);
        sem_close(sem);
        sem_unlink(sem_file_name);

	printf("backend woken up\n");
        instrument_args.state = BACK_END_INITIAL;

        _mm_sfence();
        instrument_args.ringBuffer = ringBuffer;
        mangosteen_instrument_init_front_end();


        printf("Backend done remaping memory, here is a list:");
        region_table_print(regionTable);
        printf("starting consumer\n");
        run_consumer(ringBuffer, regionTable);
    }
}

