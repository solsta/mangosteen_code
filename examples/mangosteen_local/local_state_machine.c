/**
 * This is the most basic example of Mangosteen API usage.
 * It demonstrates how to use instrument_start and instrument_stop
 * to persist memory operations.
 * On recovery the content of test_string will be printed on line 45.
 */

#include <string.h>
#include <sys/mman.h>
#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/dr_annotations.h"
#include "stdio.h"
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <emmintrin.h>

#define MEMORY_BLOCK_SIZE 4096

const char *test_string = "It is not the responsibility of knights errant to discover whether the afflicted, "
                          "the enchained and the oppressed whom they encounter on the road are reduced to these circumstances "
                          "and suffer this distress for their vices, or for their virtues: "
                          "the knight's sole responsibility is to succour them as people in need, "
                          "having eyes only for their sufferings, not for their misdeeds.";

char *test_string_small = "abcdef";
static char *arr;

void initializeStateMachine(){
    const char *file_path = "memoryBlock.txt";
    int fd = open(file_path, O_RDWR | O_CREAT, 0666);
    assert(ftruncate(fd, MEMORY_BLOCK_SIZE)!=-1);
    assert(fd != -1);

    arr = mmap(NULL, MEMORY_BLOCK_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE, fd, 0);
    bzero(arr,MEMORY_BLOCK_SIZE);
}

int main() {
    // Application specific
    initializeStateMachine();

    //Mangosteen initialization
    mangosteen_args mangosteenArgs;
    mangosteenArgs.mode = SINGLE_THREAD;
    initialise_mangosteen(&mangosteenArgs);

    printf("Array content: %s\n", arr); // On recovery this will print out test string.
    for (unsigned long i =0; i < strlen(test_string); i++){
        printf("%c", arr[i]);
    }
    printf("\n");
    printf("arr[0] addr: %p\n", arr);
    printf("PID: %d\n", getpid());
    _mm_sfence();
    instrument_start(); // Tells Mangosteen to start persistent memory transaction.
    instrumented_memcpy(arr, test_string, strlen(test_string)); // Application specific
    _mm_sfence();
    instrument_stop();  // Tells Mangosteen to commit persistent memory transaction.
    printf("Array content: %s\n", arr);
    return 0;
}
