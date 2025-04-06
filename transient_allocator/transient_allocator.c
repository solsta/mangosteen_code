//
// Created by se00598 on 01/11/23.
//

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <strings.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <string.h>
#include "transient_allocator.h"

#define MMAP_BLOCK_SIZE 65536
#define MAX_BLOCK_POW 16
#define POINTER_SIZE 8
static size_t MAX_ALLOC;

int get_block_index(size_t size);

typedef struct memory_node{
    int size;
    struct memory_node *next;
    void *usable_space;
}memory_node;

typedef struct memory_node_list{
    int block_size;
    memory_node *first;
}memory_node_list;

typedef struct transient_allocator{
    memory_node_list pMemoryNodeList[MAX_BLOCK_POW];
}transient_allocator;

void initialise_allocator(transient_allocator *transientAllocator){
    MAX_ALLOC = (size_t)pow(2,MAX_BLOCK_POW);
    bzero(transientAllocator->pMemoryNodeList, (POINTER_SIZE * MAX_BLOCK_POW));
    for(int i=0; i < MAX_BLOCK_POW; i++){
        transientAllocator->pMemoryNodeList[i].first = NULL;
        int block_size = 1 << i;
        transientAllocator->pMemoryNodeList[i].block_size = block_size;
    }
}

int get_block_index(size_t size){
    assert(size < MAX_ALLOC);
    size_t size_aligned_to_block_size = (size_t)pow(2,0);
    int index = 0;
    while(size_aligned_to_block_size < size){
        size_aligned_to_block_size *= 2;
        index+=1;
    }
    printf("Size : %zu index %d\n", size, index);
    return index;
}



// entry* = start
// entrySize
// next
// usableSpace


#define DEBUG
void add_memory_blocks_to_list(memory_node_list *memoryNodeList){
    char *addr = mmap(NULL, MMAP_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    //Continue....

    memcpy(addr, "abc", 4);
#ifdef DEBUG
    printf("mmap is ok\n");
#endif
    int entrySize = memoryNodeList->block_size + (int)(sizeof (int) + 8);
    printf("Entry size: %d\n", entrySize);

    memory_node *iterator = NULL;

    memoryNodeList->first = (memory_node *) addr;
#ifdef DEBUG
    printf("64\n");
#endif
    memcpy(&memoryNodeList->first->size, &memoryNodeList->block_size, sizeof (int));
#ifdef DEBUG
    printf("68\n");
#endif
    int offset = (int)sizeof (int) + 8 + memoryNodeList->block_size;
    memoryNodeList->first->next = (memory_node *) (addr + offset);;
    memoryNodeList->first->usable_space = addr + sizeof (int) + 8;


#ifdef DEBUG
    printf("entering a loop\n");
#endif
    while(true){
        if((offset+entrySize) < MMAP_BLOCK_SIZE){
            iterator = (memory_node *) (addr + offset);
            memcpy(&iterator->size, &memoryNodeList->block_size,sizeof (int));
            iterator->next = (memory_node *) (addr + offset + entrySize);
            iterator->next->next = NULL;
            iterator->usable_space = ((char*)iterator + sizeof (int) + 8);
            offset+=entrySize;
            if(offset + entrySize > MMAP_BLOCK_SIZE){
                iterator->next = NULL;
            }
#ifdef DEBUG
            //printf("added a block\n");
#endif
        } else{
            //iterator->next
            // TODO add this leftover chunk to corresponding list
            break;
        }
    }
#ifdef DEBUG
    printf("addr start %s\n", addr);
    memory_node *currentNode = memoryNodeList->first;
    int counter = 0;
    while(currentNode!=NULL){
        printf("Block %d start: %p size: %d next: %p\n", counter, currentNode, currentNode->size, currentNode->next);
        currentNode = currentNode->next;
        counter++;
    }
#endif
}

bool list_is_empty(memory_node_list *list){
    if(list->first == NULL){
        return true;
    }
    return false;
}

void *pop_memory_block(memory_node_list *list){
    memory_node *oldFirst = list->first;
    void *returnAddress = list->first->usable_space;
    list->first = list->first->next;
    printf("Pop block: old first: %p, usable space : %p new first %p \n", oldFirst, oldFirst->usable_space, list->first);
    //printf("Pop block: old first usable space: %p, new first usable space %p \n", , list->first->usable_space);
    return returnAddress;
}

void *alloc(transient_allocator *transientAllocator, size_t size){
    int listIndex = get_block_index(size);
    memory_node_list *list = &transientAllocator->pMemoryNodeList[listIndex];
    if(list_is_empty(list)){
        // MMAP MEMORY
        add_memory_blocks_to_list(list);
    }
    return pop_memory_block(list);
}

void push_block(memory_node_list *list, void *ptr){
    printf("Returning ptr: %p\n", ptr);
    memory_node *node = (memory_node *) ((char *) ptr - (sizeof(int) + sizeof(void *)));
    printf("Pushing block: %p\n", node);
    if(list_is_empty(list)){
        list->first = node;
        list->first->next = NULL;
        printf("Added first node of size : %d\n", node->size);
    } else{
        memory_node *iterator = list->first;
        while (iterator->next != NULL){
            iterator = iterator->next;
        }
        iterator->next = node;
        node->next = NULL;
    }
}

void free(transient_allocator *transientAllocator, void *ptr){
    memory_node *node = (memory_node *) ((char *) ptr - (sizeof(int) + sizeof(void *)));
    printf("Free node: %p size: %d\n", node, node->size);
    int listIndex = get_block_index(node->size);
    memory_node_list *list = &transientAllocator->pMemoryNodeList[listIndex];
    push_block(list, ptr);
}

int get_list_length(memory_node_list *list){
    if(list_is_empty(list)){
        printf("list is empty\n");
        return 0;
    }
    int count = 0;
    memory_node *node = list->first;

    while(node != NULL){
        count+=1;
        node = node->next;
    }
    printf("list has %d elements\n", count);
    return count;
}

int main(){

    // Test 1 Block creation.
    memory_node_list memoryNodeList;
    memoryNodeList.first = NULL;
    memoryNodeList.block_size = 16384;
    add_memory_blocks_to_list(&memoryNodeList);

    int numberOfBlocks = MMAP_BLOCK_SIZE / (memoryNodeList.block_size + sizeof (memory_node));
    printf("Number of blocks: %d\n", numberOfBlocks);

    void *memoryBlockPointers[numberOfBlocks];
    int blocksRemaining = numberOfBlocks;
    for(int i = 0; i < numberOfBlocks; i++){
        printf("I: %d\n", i);
        assert(get_list_length(&memoryNodeList) == blocksRemaining);
        memoryBlockPointers[i] = pop_memory_block(&memoryNodeList);
        blocksRemaining-=1;
        printf("Usable space: %p\n", memoryBlockPointers[i]);
    }
    printf("---\n");
    for(int i =0 ; i < numberOfBlocks; i++){
        printf("block %d addr: %p\n", i, memoryBlockPointers[i]);
    }
    printf("---\n");
    for(int i = 0; i < numberOfBlocks; i++){
        printf("I: %d, lenght: %d\n", i,get_list_length(&memoryNodeList));
        printf("saved address: %p\n", memoryBlockPointers[i]);
        assert(get_list_length(&memoryNodeList) == i);
        push_block(&memoryNodeList,memoryBlockPointers[i]);
    }

    // TODO test that all blocks are available again
    for(int i =0 ; i < numberOfBlocks; i++){
        memory_node *iterator = memoryNodeList.first;
        while(true){
            if(memoryBlockPointers[i] == iterator->usable_space){
                break;
            }
            iterator = iterator->next;
            if(iterator == NULL){
                assert(false);
            }
        }
        //assert(false);
    }

    // Test Alloc and free
    transient_allocator transientAllocator;
    initialise_allocator(&transientAllocator);
    void *ptr = alloc(&transientAllocator,8192);
    assert(get_block_index(8192) == 13);
    memory_node *node = (memory_node *) ((char *) ptr - sizeof(int) - 8);
    assert(node->size == 8192);
    return 0;
}
