//
// Created by se00598 on 18/08/23.
//

#include "region_table.h"
#include <libpmem.h>
#include <stddef.h>
#include <assert.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "../configuration.h"
#include "../ring_buffer/ring_buffer.h"



region_table *region_table_create_or_open(ring_buffer *ringBuffer) {
    int is_pmem;
    size_t mapped_len;
    region_table *regionTable;

    bool create = true;
    int fd;
     if(access(REGION_TABLE_FILE_NAME, F_OK) != 0){
#ifdef DEBUG_REGION_TABLE
         printf("region table does not exist\n");
#endif
         fd = open(REGION_TABLE_FILE_NAME, O_RDWR | O_CREAT, 0666);
         assert(ftruncate(fd, (INITIAL_TABLE_SIZE+1)*64) != -1);
} else{
         create = false;
         fd = open(REGION_TABLE_FILE_NAME, O_RDWR, 0666);
     }



   // } else{
       // printf("region table does not exists\n");
    //}
    regionTable = mmap(NULL, INITIAL_TABLE_SIZE*64, PROT_WRITE | PROT_READ,
                         MAP_SHARED, fd, 0);
    assert(regionTable != MAP_FAILED);
    //printf("Creating region table\n");
    /*
    regionTable = pmem_map_file(REGION_TABLE_FILE_NAME, 1024*64, PMEM_FILE_CREATE | MAP_SHARED,
                                0666,
                                &mapped_len, &is_pmem);
*/

/*
    if(regionTable == NULL){
        printf("Failed to create region table file, trying to open it.\n");
        regionTable = pmem_map_file(REGION_TABLE_FILE_NAME, 0, 0,
                                    0666,
                                    &mapped_len, &is_pmem);
        if(regionTable == NULL){
            printf("Failed to open region table file.\n");
            exit(1);
        }
    }
    */
#ifdef DEBUG_REGION_TABLE
    printf("Table created, initializing.\n");
#endif
    if(create) {
        regionTable->currentTableSize = INITIAL_TABLE_SIZE;
        regionTable->number_of_entries = 0;
        regionTable->table_commit_index = 0;
#ifdef DEBUG_REGION_TABLE
        printf("Initialized values\n");
#endif
    } else{
        regionTable->number_of_entries = ringBuffer->commit_metadata.numberOfRegions;
    }
    regionTable->number_of_new_entries = 0;
    regionTable->needsSync = false;
#ifdef DEBUG_REGION_TABLE
    printf("Region table created\n");
#endif
    return regionTable;
}

void get_file_name_for_sequence_number(char *fileName, uint64_t sequenceNumber){

    char *directoryPath = MEMORY_DIRECTORY_FILE_PATH;
    bzero(fileName,MAX_FILE_NAME_SIZE);
    sprintf(fileName, "%smem_file_%lu", directoryPath, sequenceNumber);
}

int create_pmem_file(uint64_t size){
    static int sequenceNumber = 0;

    char fileName[MAX_FILE_NAME_SIZE];
    get_file_name_for_sequence_number(fileName, sequenceNumber);
#ifdef DEBUG_REGION_TABLE
    printf("File name : %s\n", fileName);
#endif
    int fd;// = open(fileName, O_RDWR | O_CREAT, 0666);
    fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    //assert(ftruncate(fd, size) != -1);
    //posix_fallocate(fd, 0, size);
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        close(fd);
        exit(1);
    }
        // Retrieve file information
struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        exit(1);
    }
    assert(fd != -1);
    sequenceNumber+=1;
    return fd;
}

void region_table_commit(region_table *regionTable){
    __asm__ volatile("mfence" ::: "memory");
    regionTable->number_of_entries = regionTable->uncommited_number_of_entries;
    pmem_persist(&regionTable->number_of_entries, sizeof(uint64_t));
}

void region_table_add_region_record(region_table *regionTable, void * addr, uint64_t size){
    // TODO resizing
#ifdef DEBUG_REGION_TABLE
    printf("region_table_create_memory_region l72\n");
    printf("N of entries: %lu, Table size: %lu\n", regionTable->uncommited_number_of_entries, regionTable->currentTableSize);
#endif
    assert(regionTable->uncommited_number_of_entries < regionTable->currentTableSize);
#ifdef DEBUG_REGION_TABLE
    printf("inserting region in to region table addr: %p, size: %lu\n", addr, size);
#endif

    regionTable->regionTable[regionTable->uncommited_number_of_entries].address = addr;
    regionTable->regionTable[regionTable->uncommited_number_of_entries].mmap_size = size;
    regionTable->regionTable[regionTable->uncommited_number_of_entries].file_index = regionTable->uncommited_number_of_entries;
    regionTable->regionTable[regionTable->uncommited_number_of_entries].entry_commit_index = regionTable->table_commit_index + 1;
#ifdef DEBUG_REGION_TABLE
    printf("Inserted\n");
#endif
    pmem_flush(&regionTable->regionTable[regionTable->uncommited_number_of_entries],64);
#ifdef DEBUG_REGION_TABLE
    printf("Flushed\n");
#endif
    regionTable->uncommited_number_of_entries+=1;
}

// TODO Verify if any memory regions are maped outside of combiner
// TODO verify that no entries are added to Redo/HashSet outside of combiner sessions
void region_table_create_memory_region(region_table *regionTable, void * addr, uint64_t size){
   // if(size == 32){ //size == 8392704 |
        // TODO FIND OUT WHAT THIS FILE IS printf("Skipping thread related mmap %lu\n", size);
    //} else {

        region_table_add_region_record(regionTable, addr, size);
#ifdef DEBUG_REGION_TABLE
        printf("Creating file of size: %lu\n", size);
#endif
        int fd = create_pmem_file(size);
#ifdef DEBUG_REGION_TABLE
        printf("Mmaping to addr : %p\n", addr);
#endif
        void *returnedMmapAddress = mmap(addr, size, PROT_WRITE | PROT_READ,
                                         MAP_FIXED | MAP_SHARED_VALIDATE, fd, 0);
        assert(returnedMmapAddress == addr);
#ifdef DEBUG_REGION_TABLE
        printf("Successfully mmaped NUmber of regions is: %lu\n", regionTable->number_of_entries);
#endif
   // }
    //return returnedMmapAddress;
}

void region_table_print(region_table *regionTable){
    for(int i = 0; i < regionTable->number_of_entries; i++){
        printf("Region: %d addr: %p size:%lu\n", i, regionTable->regionTable[i].address, regionTable->regionTable[i].mmap_size);
    }
}

void region_table_restore_consumer(region_table *regionTable){
#ifdef DEBUG_REGION_TABLE
    printf("Table content:\n");
    for(uint64_t i = 0; i < regionTable->number_of_entries; i++){
        region_table_entry *regionTableEntry = &regionTable->regionTable[i];
        printf("Entry 1: addr: %p, size: %lu, file_index: %lu\n", regionTableEntry->address, regionTableEntry->mmap_size, regionTableEntry->file_index);
    }
#endif
    for(uint64_t i = 0; i < regionTable->number_of_entries; i++){
        int prot = PROT_READ | PROT_WRITE ;
        int flags;
        if(regionTable->regionTable[i].mmap_size == 32768 | regionTable->regionTable[i].mmap_size == 16384){
            // DO THEY FAIL BECAUSE DIFFERENT PID OR BECAUSE I AM USING WRONG FLAGS?
            flags = MAP_FIXED | MAP_PRIVATE | MAP_SYNC;
            //continue;
        } else{
            flags = MAP_FIXED | MAP_SHARED | MAP_SYNC;
        }
        char fileName[MAX_FILE_NAME_SIZE];
        get_file_name_for_sequence_number(fileName, i);
#ifdef DEBUG_REGION_TABLE
           printf("File name: %s\n", fileName);
#endif
        int fd = open(fileName, O_RDWR, 0666);
        if(fd == -1){
            printf("ERROR OPENING FILE\n");
        }
        region_table_entry *regionTableEntry = &regionTable->regionTable[i];
        //printf("Unmaping\n");
        //munmap(regionTableEntry->address, regionTableEntry->mmap_size);
#ifdef DEBUG_REGION_TABLE
        printf("Opening file: %s which corresponds to region %p with size: %lu\n",fileName, regionTableEntry->address, regionTableEntry->mmap_size);
#endif
        void *returnedMmapAddress = mmap(regionTableEntry->address, regionTableEntry->mmap_size, prot,
                                         flags, fd, 0);
#ifdef DEBUG_REGION_TABLE
        printf("Returned address: %p\n", returnedMmapAddress);
#endif
        assert(returnedMmapAddress == regionTableEntry->address);
    }
}

void region_table_restore_application(region_table *regionTable){
    //TODO wait until backend is finished
    for(uint64_t i = 0; i < regionTable->number_of_entries; i++){
        char fileName[MAX_FILE_NAME_SIZE];
        get_file_name_for_sequence_number(fileName, i);
        int fd = open(fileName, O_RDWR, 0666);
        void * destinationAddr = regionTable->regionTable[i].address;
        uint64_t size = regionTable->regionTable[i].mmap_size;
        void *sourceAddr = mmap(NULL, size, PROT_WRITE | PROT_READ,
                                MAP_FIXED | MAP_SHARED, fd, 0);
        memcpy(destinationAddr,sourceAddr,size);
        munmap(sourceAddr,size);
        close(fd);
    }
}
