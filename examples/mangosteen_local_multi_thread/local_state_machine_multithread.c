/**
 * This example demonstrates how Mangosteen can be used
 * in an environment with multiple threads.
 * Each application thread must make a call to mangosteen_initialize_thread() in order
 * for Mangosteen to ta be aware of it's existence.
 * execute_using_flat_combining_no_rpc() expects a pointer to a serialized application-specific data structure.
 * This data structure is later passed to isReadOnly and processRequest methods, which need to be
 * implemented by application developer.
 * isReadOnly is a classifier for read only and update commands.
 * processCommand is application specific method for deserializing and executing commands that
 * were submitted to Mangosteen.
 */
#include <string.h>
#include <sys/mman.h>
#include "stdio.h"
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include "../../mangosteen_instrumentation.h"
#include "../../flat_combining/flat_combining.h"

#define MEMORY_BLOCK_SIZE 4096
#define COPY_CHARACTER 0
#define PRINT_STATS 1

char *test_string = "It is not the responsibility of knights errant to discover whether the afflicted, "
                          "the enchained and the oppressed whom they encounter on the road are reduced to these circumstances "
                          "and suffer this distress for their vices, or for their virtues: "
                          "the knight's sole responsibility is to succour them as people in need, "
                          "having eyes only for their sufferings, not for their misdeeds.";

static size_t number_of_characters_to_copy;
size_t current_character_index;

static char *arr;

void initializeStateMachine(){
    const char *file_path = "memoryBlock.txt";
    int fd = open(file_path, O_RDWR | O_CREAT, 0666);
    assert(ftruncate(fd, MEMORY_BLOCK_SIZE)!=-1);
    assert(fd != -1);

    arr = mmap(NULL, MEMORY_BLOCK_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE, fd, 0);
    bzero(arr,MEMORY_BLOCK_SIZE);
}

void *runWorkerThread(void *arg){
    mangosteen_initialize_thread();
    serialized_app_command serializedAppCommand;

    while(current_character_index < number_of_characters_to_copy){
        // Random read/write
        int randomBit = rand() % 2;
        if (randomBit == 1){
            serializedAppCommand.op_type = PRINT_STATS;
        }else{
            serializedAppCommand.op_type = COPY_CHARACTER;
        }
        execute_using_flat_combining_no_rpc(&serializedAppCommand);
    }
    return NULL;
}

/**
 * Application specific method
 * This spins up a number of worker threads that need to do some read abd update operations.
 */
void executeTextCopy(){
    srand(time(NULL));
    number_of_characters_to_copy = strlen(test_string);
    current_character_index = 0;
    printf("Number of characters to be copied: %zu\n", number_of_characters_to_copy);
    pthread_t threads[NUMBER_OF_THREADS];
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        pthread_create(&threads[i], NULL, runWorkerThread, NULL);
    }
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    assert(strcmp(test_string, arr) == 0);
}

/**
 * Application specific method
 * Since this method is classified as update, it will be executed by Mangosteen under exclusive lock.
 * This method will also atomically persist memory operations and will make them available on recovery.
 * Mangosteen will internally call instrument_start() and instrument_stop() to ensure that.
 */
void copyTextBlock(){
    if(current_character_index <= number_of_characters_to_copy){
        arr[current_character_index] = test_string[current_character_index];
        current_character_index+=1;
    }
}
/**
 * Application specific method
 * This method is classified as read only and will be executed under a shared reader lock.
 * No memory operations are persisted.
 */
void printProgress(){
#ifdef DEBUG
    printf("Done: %zu Remaining: %zu\n", current_character_index, number_of_characters_to_copy-current_character_index);
#endif
    assert(number_of_characters_to_copy-current_character_index >= 0);
}

/**
 * Mangosteen callback function
 * A simple example of how a command can be deserialized and classified as read only or update.
 * @param serializedAppCommand
 * @return
 */
bool isReadOnly(serialized_app_command *serializedAppCommand){
#ifdef DEBUG
    printf("isReadOnly\n");
#endif
    if (serializedAppCommand->op_type == COPY_CHARACTER){
        return false;
    }
    return true;
}

/**
 * Mangosteen callback function
 * A simple example of how a command can be deserialized and processed by the application.
 * @param serializedAppCommand
 */
void processCommand(serialized_app_command *serializedAppCommand){
#ifdef DEBUG
    printf("processCommand \n");
#endif
    if (serializedAppCommand->op_type == COPY_CHARACTER){
        copyTextBlock();
        return;
    }
    if(serializedAppCommand->op_type == PRINT_STATS){
        printProgress();
        return;
    }
    printf("ERROR: COMMAND HAS NOT BEEN IMPLEMENTED :%d?\n", serializedAppCommand->op_type);
    exit(1);
}

int main(int argc, char *argv[]) {
    // Application specific
    initializeStateMachine();

    //Mangosteen initialization
    mangosteen_args mangosteenArgs;
    mangosteenArgs.processRequest = processCommand;
    mangosteenArgs.isReadOnly = isReadOnly;
    mangosteenArgs.mode = MULTI_THREAD;
    initialise_mangosteen(&mangosteenArgs);

    printf("Array content: %s\n", arr); // On recovery this will print out test string.

    // Application specific
    executeTextCopy();

    return 0;
}