//
// Created by se00598 on 18/08/23.
//

#ifndef MANGOSTEEN_INSTRUMENTATION_REGION_TABLE_H
#define MANGOSTEEN_INSTRUMENTATION_REGION_TABLE_H

#include <stdint-gcc.h>
#include <stdbool.h>
#include "../ring_buffer/ring_buffer.h"
#define REGION_TABLE_FILE_NAME "/home/vagrant/mangosteen_pmem_data/test_outputs/region_table"
//uint64_t initialTableSize =  1023;
#define MAX_FILE_NAME_SIZE 500

#define INITIAL_TABLE_SIZE 65536

typedef struct regionTableEntry{
    void *address;
    uint64_t file_index;
    uint64_t mmap_size;
    uint64_t entry_commit_index;
    char padding[32];
} region_table_entry;

typedef struct regionTable{
    uint64_t table_commit_index;
    uint64_t currentTableSize;
    uint64_t number_of_entries;
    uint64_t uncommited_number_of_entries;
    uint64_t number_of_new_entries;
    region_table_entry newEntries[16];
    _Atomic bool needsSync;
    char padding[32];
    region_table_entry regionTable[INITIAL_TABLE_SIZE];
} region_table;
void region_table_print(region_table *regionTable);
void region_table_create_memory_region(region_table *regionTable, void *addr, uint64_t size);
region_table *region_table_create_or_open(ring_buffer *ringBuffer);
void region_table_restore_consumer(region_table *regionTable);
void region_table_restore_application(region_table *regionTable);
void get_file_name_for_sequence_number(char *fileName, uint64_t sequenceNumber);
void region_table_add_region_record(region_table *regionTable, void * addr, uint64_t size);
void region_table_commit(region_table *regionTable);
int create_pmem_file(uint64_t size);
#endif //MANGOSTEEN_INSTRUMENTATION_REGION_TABLE_H
