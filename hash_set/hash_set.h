//
// Created by se00598 on 01/08/23.
//

#ifndef MANGOSTEEN_INSTRUMENTATION_HASH_SET_H
#define MANGOSTEEN_INSTRUMENTATION_HASH_SET_H

#define HASH_SET_ADDRESS_COVERAGE 64
//#define ADDRESS_HASH_TABLE_INITIAL_SIZE 1024

#if HASH_SET_ADDRESS_COVERAGE == 256
#define MASK_SHIFT 8
#endif

#if HASH_SET_ADDRESS_COVERAGE == 128
#define MASK_SHIFT 7
#endif

#if HASH_SET_ADDRESS_COVERAGE == 64
#define MASK_SHIFT 6
#endif

typedef struct {
    int numberOfEntries; //Each entry is a PAYLOAD_SIZE byte aligned region of memory
    int numberOfCollisions; // All hash collisions
    int numberOfDuplicates; // Collisions caused by duplicate setOfEntries. Subset of [numberOfCollisions].
    // ^ we don't know which ones are duplicates and which ones fit into aligned chunk of memory;
    // Can potentially count hits for each entry, but will also need to keep track of their sizes.
    int numberOfResizes;
} address_hash_set_stats;


typedef struct {
    char *alignedAddress;
} address_hash_set_entry;


typedef struct {
    int size;
    double resizeThreshold;
    int maxElementsBeforeResize;
    address_hash_set_stats stats;
    address_hash_set_entry *setOfEntries;
    unsigned long last_used_index;
} address_hash_set;

extern address_hash_set *address_hash_set_add_redo_log_stub(address_hash_set *addressHashSet, const char *address, unsigned long size);
address_hash_set *address_hash_set_initialise(int size, double resizeThreshold);
#endif //MANGOSTEEN_INSTRUMENTATION_HASH_SET_H
