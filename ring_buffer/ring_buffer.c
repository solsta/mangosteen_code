//
// Created by se00598 on 10/07/23.
//

#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ring_buffer.h"

// Pause to prevent excess processor bus usage
#if defined( __sparc )
#define Pause() __asm__ __volatile__ ( "rd %ccr,%g0" )
#elif defined( __i386 ) || defined( __x86_64 )
#define Pause() __asm__ __volatile__ ( "pause" : : : )
#endif




__attribute__((aligned(64))) uint64_t number_of_mmaped_memory_regions;
__attribute__((aligned(64))) ring_buffer_entry *buf_ptr;

//#define DEBUG_PRINT
//#define TEST_ONLY_LOGGING_PART
//#define PERSISTENCE_OPERATIONS
const unsigned long pmem_file_size = (ring_buffer_size*sizeof(ring_buffer_entry)+32);

bool is_a_memory_tag(char *msg){
    return (strncmp(msg, MEMORY_METADATA_START_TAG, 8) == 0);
}

bool has_more_map_entries(char *msg){
    return (strncmp(msg, MEMORY_METADATA_END_TAG, 8) != 0);
}

bool has_more_redo_entries(char *msg){
    return (strncmp(msg, REDO_LOG_END_TAG, 8) != 0);
}

uint64_t get_global_head_value(ring_buffer *ringBuffer){
    return ringBuffer->last_persisted_head;
}

uint64_t get_global_tail_value(ring_buffer *ringBuffer){
    return ringBuffer->last_persisted_tail;
}

void set_global_head_value(uint64_t thread_local_head,ring_buffer *ringBuffer){
     ringBuffer->last_persisted_head = thread_local_head;
}

void set_global_tail_value(uint64_t thread_local_tail,ring_buffer *ringBuffer){
    ringBuffer->last_persisted_tail = thread_local_tail;
}
int fd;
void close_ring_buffer(ring_buffer *ringBuffer){
    munmap(ringBuffer,pmem_file_size);
    close(fd);
}

ring_buffer  *ring_buffer_create_and_initialise(ring_buffer *ringBuffer){
    size_t mapped_len;
    int is_pmem;

     // MAP AS NORMAL FILE

    bool recover = false;
    if(access(RING_BUFFER_FILE_PATH, F_OK) != 0){
        fd = open(RING_BUFFER_FILE_PATH, O_RDWR | O_CREAT, 0666);
        assert(ftruncate(fd, pmem_file_size) != -1);
    } else{
        recover = true;
        fd = open(RING_BUFFER_FILE_PATH, O_RDWR, 0666);
    }


    ringBuffer = mmap(NULL, pmem_file_size, PROT_WRITE | PROT_READ,
                                       MAP_SHARED, fd, 0);

    assert(ringBuffer!= NULL);
    /* create a pmem file and memory map it */
    /*
    if ((ringBuffer = pmem_map_file(RING_BUFFER_FILE_PATH, pmem_file_size, PMEM_FILE_CREATE, 0666,
                                    &mapped_len, &is_pmem)) == NULL) {
        perror("pmem_map_file");
        exit(1);
    }
*/
    if(!recover){
#ifdef DEBUG_RING_BUFFER
    printf("RB address: %lu, %p\n", (uint64_t)ringBuffer, ringBuffer);
#endif
    uint64_t addr = (uint64_t)ringBuffer;
    assert(addr % 64 == 0);
    ringBuffer->remote_head=0;
    ringBuffer->remote_tail=0;
    addr = (uint64_t)ringBuffer->remote_head;
    assert(addr % 128 == 0);
    addr = (uint64_t)ringBuffer->remote_tail;
    assert(addr % 128 == 0);

    ringBuffer->last_persisted_head = 0;
    ringBuffer->last_persisted_tail = 0;
    global_local_tail=0;
    global_local_head=0;
    number_of_mmaped_memory_regions=0;
    ringBuffer->producer_addr = ringBuffer;

    buf_ptr = ringBuffer->buffer;

    ringBuffer->commit_metadata.ringBufferHead = 0;
    ringBuffer->commit_metadata.numberOfRegions = 0;
    pmem_persist(&ringBuffer->commit_metadata,8);

    pmem_persist(ringBuffer->producer_addr, 8);
#ifdef DEBUG_RING_BUFFER
    printf("Ring buffer initialised\n");
#endif
    } else{
        global_local_tail = ringBuffer->remote_tail;
        global_local_head = ringBuffer->remote_head;
        ringBuffer->last_persisted_head = ringBuffer->remote_head;
        ringBuffer->last_persisted_tail = ringBuffer->remote_tail;
        buf_ptr = ringBuffer->buffer;
        number_of_mmaped_memory_regions = ringBuffer->commit_metadata.numberOfRegions;
    }

    return ringBuffer;
}

void execute_flush(void *addr){
    //TODO flush once we have a full cache line packed
#ifdef PERSISTENCE_OPERATIONS
    pmem_flush(addr, 64);
#endif
}

ring_buffer *map_ring_buffer_file_to_correct_address(){
    //int fd;
    if(access(RING_BUFFER_FILE_PATH, F_OK) != 0){
        fd = open(RING_BUFFER_FILE_PATH, O_RDWR | O_CREAT, 0666);
        assert(ftruncate(fd, pmem_file_size) != -1);
    } else{
        fd = open(RING_BUFFER_FILE_PATH, O_RDWR, 0666);
    }
    if(fd == -1){
        printf("Backend: ERROR opening rb file\n");
    }
#ifdef DEBUG_RING_BUFFER
    printf("Opened RB file\n");
#endif
     // READ THE MMAPING ADDRESS
    ring_buffer *ringBuffer = mmap(NULL, pmem_file_size, PROT_WRITE | PROT_READ,
                        MAP_SHARED, fd, 0);
    void *rbAddress = ringBuffer->producer_addr;
    munmap(ringBuffer, pmem_file_size);
    ringBuffer = mmap(rbAddress, pmem_file_size, PROT_WRITE | PROT_READ,
         MAP_FIXED | MAP_SHARED, fd, 0);
    assert(ringBuffer == rbAddress);
#ifdef DEBUG_RING_BUFFER
    printf("Backend: Ring buffered opened and mmaped\n");
#endif

    return ringBuffer;
}

ring_buffer *ring_buffer_open(bool init){
#ifdef DEBUG_RING_BUFFER
    printf("Opening ring buffer\n");
#endif
    size_t mapped_len;
    int is_pmem;
    ring_buffer *ringBuffer;
    ringBuffer = map_ring_buffer_file_to_correct_address();

    ringBuffer->remote_head = ringBuffer->commit_metadata.ringBufferHead;
    pmem_persist(&ringBuffer->remote_head, 8);

    global_local_tail = ringBuffer->remote_tail;

    global_local_head = ringBuffer->remote_head;
#ifdef DEBUG_RING_BUFFER
    printf("Producer rb addr: %p; consumer: %p\n", ringBuffer, ringBuffer->producer_addr);
    printf("Tail: %lu Head: %lu\n", ringBuffer->remote_tail, ringBuffer->remote_head);
#endif
    //buf_ptr = ringBuffer->buffer;
    //number_of_mmaped_memory_regions = ringBuffer->commit_metadata.numberOfRegions;

    return ringBuffer;
}

void reset_mmaped_regions_counter(){
    number_of_mmaped_memory_regions = 0;
}

// *payload has to be a pointer to 16 byte struct or message
// TODO change this method to do what it says (I think it does that anyway)
void ring_buffer_enqueue_mmap_entry(ring_buffer *ringBuffer, ring_buffer_map_entry *ringBufferMapEntry, uint64_t *thread_local_head, uint64_t *thread_local_tail){
#ifdef DEBUG_RING_BUFFER
    printf("Enqueue mmap entry %lu\n",*thread_local_tail);
#endif
    uint64_t nextTail = (*thread_local_tail + 1);
    if(nextTail == RING_BUFFER_SIZE){
        nextTail = 0;
    }
    while(nextTail == *thread_local_head){
        *thread_local_head = ringBuffer->remote_head;
#ifdef DEBUG_RING_BUFFER
        printf("Looping at index %lu\n",nextTail);
#endif
    }

    char *start = (char *) (buf_ptr + *thread_local_tail);
    ring_buffer_map_entry *destination = (ring_buffer_map_entry *) start;
    //ring_buffer_entry *ringBufferEntry = (ring_buffer_entry *)start;
#ifdef DEBUG_RING_BUFFER
    printf("Copying\n");
#endif

    //memcpy(&destination->entry_type, (const void *) (bool) 1, 1);

    destination->addr = ringBufferMapEntry->addr;
    //memcpy(start+1, &ringBufferMapEntry->addr, 8);

    destination->size = ringBufferMapEntry->size;
    destination->entry_type = ringBufferMapEntry->entry_type;
    //memcpy(start+9, &ringBufferMapEntry->size, 8);
    pmem_flush(destination, 64);
//#ifdef DEBUG_PRINT
#ifdef DEBUG_RING_BUFFER
    printf("Logging to index %lu\n",*thread_local_tail);
#endif
    //printf("FRONT END LOGGED: Addr: %p Payload: %s\n", ringBufferEntry->addr, ringBufferEntry->payload);
//#endif
    *thread_local_tail = nextTail;


}

void ring_buffer_enqueue_redo_log_entry(ring_buffer *ringBuffer, void *addr,uint64_t *thread_local_head, uint64_t *thread_local_tail) {


    uint64_t nextTail = (*thread_local_tail + 1);
    if(nextTail == RING_BUFFER_SIZE){
        nextTail = 0;
    }
    while(nextTail == *thread_local_head){
        *thread_local_head = ringBuffer->remote_head;
#ifdef DEBUG_RING_BUFFER
        printf("Looping at index %lu\n",nextTail);
#endif
    }
#ifdef DEBUG_RING_BUFFER
    printf("Logging redo entry: %lu\n",nextTail);
#endif
   // printf("redo log: l236\n");
    char *start = (char *) (buf_ptr + *thread_local_tail);
    ring_buffer_entry *ringBufferEntry = (ring_buffer_entry *)start;
    ringBufferEntry->entry_type = REDO_ENTRY;
    ringBufferEntry->addr = addr;
    //memcpy(start+1, &addr, 8);
   // printf("redo log: l240\n");
#ifdef DEBUG_RING_BUFFER
    printf("FRONT END ABOUT TO LOG : Addr: %p\n", ringBufferEntry->addr);
#endif
    //printf("FRONT END ABOUT TO LOG : Addr: %p\n", ringBufferEntry->addr);
    memcpy(ringBufferEntry->payload, addr, PAYLOAD_SIZE);
//#ifdef DEBUG_PRINT
    pmem_flush(ringBufferEntry, 64);
    //printf("Logging to index %lu value: %s\n",*thread_local_tail, ringBufferEntry->payload);
   //
//#endif
    *thread_local_tail = nextTail;
    //pmem_flush(&ringBuffer->buffer[*thread_local_tail], 64);
    //execute_flush(start);
#ifdef DEBUG_RING_BUFFER
    printf("Redo entry logged ok\n");
#endif
}

void ring_buffer_commit_enqueued_entries(ring_buffer *ringBuffer, uint64_t *thread_local_head, uint64_t *thread_local_tail){

    size_t persistLength;

    uint64_t nextTail = (*thread_local_tail + 1);
    if(nextTail == RING_BUFFER_SIZE){
        nextTail = 0;
    }
    while(nextTail == *thread_local_head){
        *thread_local_head = ringBuffer->remote_head;
    }
    char *start = (char *) (buf_ptr + *thread_local_tail);
    ring_buffer_entry *ringBufferEntry = (ring_buffer_entry *)start;
    ringBufferEntry->entry_type = COMMIT_ENTRY;

    pmem_flush(ringBufferEntry, 64);
    *thread_local_tail = nextTail;
    pmem_drain(); // SFENCE
    
    ringBuffer->remote_tail = *thread_local_tail;
    pmem_flush(&ringBuffer->remote_tail, sizeof (uint64_t));
    __asm__ volatile("mfence" ::: "memory");
    
    ringBuffer->last_persisted_tail = ringBuffer->remote_tail;
}




void ring_buffer_print_info(ring_buffer *ringBuffer){
    printf("RB info: remote_tail: %lu global_local_tail: %lu remote_head: %lu global_local_head: %lu\n",
           ringBuffer->remote_tail,
           global_local_tail,
           ringBuffer->remote_head,
           global_local_head);
}


void ring_buffer_commit_dequeued_entries(ring_buffer *ringBuffer){
    ringBuffer->remote_head = global_local_head;
    pmem_flush(&ringBuffer->remote_head, sizeof (uint64_t));
    __asm__ volatile("mfence" ::: "memory");
    ringBuffer->last_persisted_head = ringBuffer->remote_head;
}

ring_buffer_entry *ring_buffer_dequeue(ring_buffer *ringBuffer){
    uint64_t head = global_local_head;
    while(head == global_local_tail){ // Empty; Wait until new entry appears.
        Pause();
        global_local_tail = ringBuffer->last_persisted_tail;
    }
    ring_buffer_entry *entry;
    entry = (ring_buffer_entry *)&ringBuffer->buffer[head];
    global_local_head+=1;
    if(global_local_head == ring_buffer_size){
        global_local_head = 0;
    }
    return entry;
}
