//
// Created by se00598 on 01/08/23.
//
#include "hash_set.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint-gcc.h>
#include <x86intrin.h>


//#define DEBUG_PRINT
//#define RUN_TESTS


/* Assembly methods */
uint64_t calculate_aligned_address(uint64_t address);
uint64_t calculate_hash_set_index(uint64_t address, uint64_t size);
bool check_if_index_is_free(uint64_t index, address_hash_set_entry *setOfEntries);
bool check_if_duplicate(uint64_t addr, uint64_t index, address_hash_set_entry *setOfEntries);
uint64_t requiredEntries(uint64_t address, uint64_t size);
void store_aligned_address(uint64_t addr, uint64_t index, address_hash_set_entry *setOfEntries);
void asm_address_hash_set_add_redo_log_stub(uint64_t addr, address_hash_set_entry *setOfEntries, uint64_t size);
/* --- */
address_hash_set * address_hash_set_add_redo_log_stub(address_hash_set *addressHashSet, const char *address, unsigned long size);
void address_hash_set_insert_aligned_block_address(address_hash_set *addressHashSet, char *alignedAddress);


address_hash_set *address_hash_set_initialise(int size, double resizeThreshold){
    address_hash_set *addressHashSet = calloc(1, sizeof (address_hash_set));
    addressHashSet->setOfEntries = calloc(1, sizeof (address_hash_set_entry)*size);
    addressHashSet->size = size;
    addressHashSet->stats.numberOfEntries = 0;
    addressHashSet->stats.numberOfCollisions = 0;
    addressHashSet->stats.numberOfDuplicates = 0;
    addressHashSet->stats.numberOfResizes = 0;
    addressHashSet->maxElementsBeforeResize = (int )(size * resizeThreshold);
    addressHashSet->resizeThreshold = resizeThreshold;
    return addressHashSet;
}


void address_hash_set_reset(address_hash_set *addressHashSet){
    bzero(&addressHashSet->setOfEntries[0], addressHashSet->size * sizeof (address_hash_set_entry));
    addressHashSet->stats.numberOfEntries = 0;
    addressHashSet->stats.numberOfCollisions = 0;
    addressHashSet->stats.numberOfDuplicates = 0;
    addressHashSet->stats.numberOfResizes = 0;
}


void address_hash_set_destroy(address_hash_set *addressHashSet){
    free(addressHashSet->setOfEntries);
    free(addressHashSet);
}


bool element_is_inserted(address_hash_set *addressHashSet, unsigned long addressHashSetIndex, char *alignedAddress){
    if(addressHashSet->setOfEntries[addressHashSetIndex].alignedAddress == 0){
        //SET this entry to current alignedAddress
        addressHashSet->setOfEntries[addressHashSetIndex].alignedAddress = alignedAddress;
        addressHashSet->stats.numberOfEntries+=1;
        addressHashSet->last_used_index = addressHashSetIndex;
#ifdef DEBUG_PRINT
        printf("FOUND free slot on first attempt\n");
#endif
        return true;
    }
    addressHashSet->stats.numberOfCollisions+=1;
    return false;
}


bool element_is_a_duplicate(address_hash_set *addressHashSet, unsigned long addressHashSetIndex, char *alignedAddress){
    //Check if the alignedAddress is the same, if so count duplicate and return
    if(addressHashSet->setOfEntries[addressHashSetIndex].alignedAddress == alignedAddress){
#ifdef DEBUG_PRINT
        printf("FOUND duplicate\n");
#endif
        //FOUND duplicate
        addressHashSet->stats.numberOfDuplicates+=1;
        return true;
    }
    return false;
}


bool addressIsProcessed(address_hash_set *addressHashSet, unsigned long addressHashSetIndex, char *alignedAddress){
    if(element_is_inserted(addressHashSet, addressHashSetIndex, alignedAddress))
        return true;
    if (element_is_a_duplicate(addressHashSet, addressHashSetIndex, alignedAddress))
        return true;
    return false;
}


address_hash_set *address_hash_set_resize(address_hash_set *originalAddressHashSet){
    int new_size = originalAddressHashSet->size*2;
    address_hash_set *resizedHashSet = address_hash_set_initialise(
            new_size,
            originalAddressHashSet->resizeThreshold);
#ifdef DEBUG_PRINT
    printf("Initialised a new hashset ");
   printf("Size: %d, Max el before resize: %d\n", resizedHashSet->size, resizedHashSet->maxElementsBeforeResize);
#endif
    assert(resizedHashSet->size == originalAddressHashSet->size*2);
    assert(resizedHashSet->maxElementsBeforeResize == (originalAddressHashSet->size*2)*originalAddressHashSet->resizeThreshold);


    resizedHashSet->stats.numberOfResizes+=originalAddressHashSet->stats.numberOfResizes + 1;
    resizedHashSet->stats.numberOfCollisions+=originalAddressHashSet->stats.numberOfCollisions;
    resizedHashSet->stats.numberOfDuplicates+=originalAddressHashSet->stats.numberOfDuplicates;
#ifdef DEBUG_PRINT
    printf("Set number of resizes\n");
#endif
    for(int i = 0; i < originalAddressHashSet->size; i++){
#ifdef DEBUG_PRINT
        printf("Checking element %d\n", i);
#endif
        if(originalAddressHashSet->setOfEntries[i].alignedAddress != 0){
#ifdef DEBUG_PRINT
            printf("Copying %p to new hashset\n", originalAddressHashSet->setOfEntries[i].alignedAddress);
#endif
            address_hash_set_insert_aligned_block_address(resizedHashSet, originalAddressHashSet->setOfEntries[i].alignedAddress);
        }
    }
    address_hash_set_destroy(originalAddressHashSet);
#ifdef DEBUG_PRINT
    printf("Finished hash set resizing\n");
#endif
    return resizedHashSet;
}




void address_hash_set_insert_aligned_block_address(address_hash_set *addressHashSet, char *alignedAddress){
    // TODO Investigate if it is worth to use hash function here to reduce collisions.
    unsigned long addressHashSetIndex = (unsigned long)alignedAddress % addressHashSet->size;
    while(true){
        if(addressIsProcessed(addressHashSet, addressHashSetIndex, alignedAddress))
            return;
        addressHashSetIndex = (addressHashSetIndex + 1) % addressHashSet->size;
        assert(addressHashSetIndex < addressHashSet->size);
    }
}
//! This will need to be implemented in assembly and inserted using Dynamorio API.
address_hash_set *address_hash_set_add_redo_log_stub(address_hash_set *addressHashSet, const char *address, unsigned long size){
    unsigned long alignedAddress;
    unsigned long bitmask = ~0UL << MASK_SHIFT;
    unsigned long range = (unsigned long)address+size;
    alignedAddress = (unsigned long)address & bitmask;
    while ( alignedAddress < range){
#ifdef DEBUG_PRINT
        printf("ADDR: %p REMAINDER: %lu Aligned ADDR: %p\n",
              address,
              reminder,
              alignedAddress);
#endif

        if(addressHashSet->stats.numberOfEntries == addressHashSet->maxElementsBeforeResize){
#ifdef DEBUG_PRINT
            printf("Number of elements: %d Max elements before resize: %d Calling resize\n",
                  addressHashSet->stats.numberOfEntries,
                  addressHashSet->maxElementsBeforeResize);
#endif
            addressHashSet = address_hash_set_resize(addressHashSet);
        }

        address_hash_set_insert_aligned_block_address(addressHashSet,(char *)alignedAddress);
        alignedAddress+=HASH_SET_ADDRESS_COVERAGE;
    }
    return addressHashSet;
}


void address_hash_set_print_elements(address_hash_set *addressHashSet){
    for(int i = 0; i < addressHashSet->size; i++){
        if(addressHashSet->setOfEntries[i].alignedAddress != 0) {
            printf("INDEX: %i ADDRESS: %p; \n",
                   i,
                   addressHashSet->setOfEntries[i].alignedAddress);
        }
    }
}


void address_hash_set_print_stats(address_hash_set *addressHashSet){
    printf("Entries: %d; Collisions: %d; Duplicates: %d; Resizes: %d\n",
           addressHashSet->stats.numberOfEntries,
           addressHashSet->stats.numberOfCollisions,
           addressHashSet->stats.numberOfDuplicates,
           addressHashSet->stats.numberOfResizes);
}