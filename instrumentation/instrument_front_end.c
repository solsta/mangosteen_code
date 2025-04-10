#include <stdio.h>
#include <string.h> /* for memset */
#include <stddef.h> /* for offsetof */
#include <stdint.h>
#include <syscall.h>
#include <sys/mman.h>
#include <libpmem.h>
#include <assert.h>
#include <x86intrin.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>


#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "drx.h"
#include "drwrap.h"
#include "dr_defines.h"
#include "instrument_front_end.h"
#include "../ring_buffer/ring_buffer.h"
#include "../region_table/region_table.h"
#include "../configuration.h"
#include "instrumenation_common.h"
#include "redo_entry_collector.h"


#ifdef RADIX_SORT
#include "../radix_sort/radix_sort.h"
#endif

#define DR_MEMPROT_SHARED 0x80
#define DR_MEMPROT_STACK 0x40

#ifdef COLLECT_REDO_LOG_METRICS
unsigned long totalMemcpy;
unsigned long totalPersist;
#endif

static ring_buffer *ringBuffer;

typedef struct {
    app_pc last_pc;
} instru_data_t;

static size_t page_size;
static client_id_t client_id;
static app_pc code_cache;
static void *mutex;    
static uint64 global_num_refs;


static void
event_exit(void);
static void
event_thread_init(void *drcontext);
static void
event_thread_exit(void *drcontext);
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating);
static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data);
static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                bool for_trace, bool translating, void *user_data);

static void
module_load_event(void *drcontext, module_data_t *mod, bool loaded);
static void synchronize_address_space(region_table *regionTable, bool initial);
static void
clean_call(void);
static void
memtrace(void *drcontext);
static void
code_cache_init(void);
static void
code_cache_exit(void);
static void
instrument_mem(void *drcontext, instrlist_t *ilist, instr_t *where, app_pc pc,
               instr_t *memref_instr, int pos, bool write, opnd_t original_opnd);



DR_EXPORT instrument_args_t instrument_args;

DR_EXPORT int mangosteen_instrument_init_front_end() {

#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR, "Instrument Init called\n");
#endif
    if (dr_app_setup_and_start() != 0) {
        dr_fprintf(STDERR, "failed to setup and start\n");
    }
    return 0;
}

int compare_uint64(const void *a, const void *b){
    const uint64_t *ai = (const uint64_t *)a;
    const uint64_t *bi = (const uint64_t *)b;
    if (*ai < *bi) {
        return -1;
    }
    if(*ai > *bi){
        return 1;
    }
    printf("Found duplicate where there should not be one!");
    assert(false);
    exit(1);
    return 0;

}


DR_EXPORT static void
instrument_start(){
#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR, "RUNNING RB CHECK\n");
#endif
    void *drcontext  = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    data->combiner = 1;

}




static bool event_filter_syscall(void *drcontext, int sysnum) {
    if(instrument_args.state == FRONT_END_RECOVERY){
        per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    }
    if (sysnum == SYS_mmap ) { //|| sysnum == SYS_munmap
        return true;
    }
    return false;
}
DR_EXPORT static void
collect_mmaps(deferred_mmap_entries *destination){
    void *drcontext  = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    int index = data->deferredMmapEntries.number_of_regions_in_this_batch;
    memcpy(destination, &data->deferredMmapEntries, sizeof (deferred_mmap_entries));
    _mm_sfence();
    data->deferredMmapEntries.number_of_regions_in_this_batch = 0;
}

static bool event_pre_syscall(void *drcontext, int sysnum)
{
    if (sysnum != SYS_mmap && sysnum != SYS_munmap) {
        return true;
    }
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);

    void* addr = (void *)(dr_syscall_get_param(drcontext, 0));

    size_t size = dr_syscall_get_param(drcontext, 1);

    data->lastSyscall.sysnum = sysnum;
    data->lastSyscall.addr = addr;
    data->lastSyscall.size = (uint64_t)size;
    return true;
}
_Atomic int *fc_writers;
static void event_post_syscall(void *drcontext, int sysnum) {


    if (sysnum != SYS_mmap && sysnum != SYS_munmap) {
        return;
    }

    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);

    if (data->lastSyscall.sysnum == SYS_mmap) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "Intercepted mmap, logging to RB\n");
#endif
        if (data->lastSyscall.addr == 0) {
            // In this case we use the returned address
            assert(dr_syscall_get_result(drcontext) != -1);
            data->lastSyscall.addr = (void *) dr_syscall_get_result(drcontext);
        }
        ring_buffer_map_entry ringBufferMapEntry;
        ringBufferMapEntry.addr = data->lastSyscall.addr;
        ringBufferMapEntry.size = data->lastSyscall.size;
        ringBufferMapEntry.entry_type = MMAP_ENTRY;

        if(data->combiner == 0){
//take rw lock

             while(true){
            if (*fc_writers == 0) {
                _Atomic int expected = 0;
                _Atomic int desired = 1;

                if (__atomic_compare_exchange(fc_writers, &expected, &desired, false,memory_order_seq_cst,memory_order_seq_cst)) {
                    break;
                }
                }
            }
            ringBufferMapEntry.entry_type =TRANSIENT_MMAP_ENTRY;
        }
#ifdef DEBUG_INSTRUMENTATION
       printf("mmap addr %p of size: %lu, added\n",ringBufferMapEntry.addr, ringBufferMapEntry.size);
#endif
       
       data->thread_local_head = get_global_head_value(ringBuffer);
       data->thread_local_tail = get_global_tail_value(ringBuffer);

#ifdef LOG_TO_RING_BUFFER
        ring_buffer_enqueue_mmap_entry(data->pmem_buffer, &ringBufferMapEntry, &data->thread_local_head,
                                       &data->thread_local_tail);
        ring_buffer_commit_enqueued_entries(data->pmem_buffer, &data->thread_local_head, &data->thread_local_tail);
#endif
        set_global_head_value(data->thread_local_head,ringBuffer);
        set_global_tail_value(data->thread_local_tail,ringBuffer);
        mprotect(ringBufferMapEntry.addr, ringBufferMapEntry.size, PROT_WRITE | PROT_READ);

        if(ringBufferMapEntry.entry_type == TRANSIENT_MMAP_ENTRY){
            *fc_writers = 0;
        }

    }
    // TODO implement munmap
}
void set_combiner_value();

DR_EXPORT static void
commit_mmaps(int count, ring_buffer_map_entry *entries){
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);


    printf("Combiner about to mmap the following deffered entries:\n");
    for(int i=0; i<count; i++){
        printf("addr : %p size: %lu\n", entries[i].addr, entries[i].size);
    }


    data->thread_local_head = get_global_head_value(ringBuffer);
    data->thread_local_tail = get_global_tail_value(ringBuffer);

    for(int i=0; i<count; i++){
#ifdef LOG_TO_RING_BUFFER
        ring_buffer_enqueue_mmap_entry(data->pmem_buffer, &entries[i], &data->thread_local_head,
                                       &data->thread_local_tail);
        ring_buffer_commit_enqueued_entries(ringBuffer,&data->thread_local_head,&data->thread_local_tail);
#endif
    }

    set_global_head_value(data->thread_local_head,ringBuffer);
    set_global_tail_value(data->thread_local_tail,ringBuffer);

data->combiner = 0;
}
DR_EXPORT static void
setCombinerToInitilise(){
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    data->combiner = 2;
}

static void processOverflowBuffer(){
    printf("Entries in overflow buffer %d\n", overflowBuffer.size);
    for(int i=0; i<overflowBuffer.size; i++){
        printf("Overflow buffer entry: %d : %p\n", i, overflowBuffer.buffer[i]);
    }
}

DR_EXPORT static void
instrumented_memcpy(void *dest, void *src, size_t size){
#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR,"memcpy arg 0: %p\n", dest);
    dr_fprintf(STDERR,"memcpy arg 1: %p\n", src);
    dr_fprintf(STDERR,"memcpy arg 0: %d\n", size);
#endif
    void *drcontext  = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    bool exectute_memcpy = false;
    if(data->combiner == 1){
            data->memcpyArray.array[data->memcpyArray.count].start = dest;
            data->memcpyArray.array[data->memcpyArray.count].end = dest+size;
            data->memcpyArray.count++;
            exectute_memcpy = true;
        memcpy(dest,src,size);
    }
}

DR_EXPORT static void
instrument_stop()
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    data->combiner = 0;
#ifdef USE_PER_BATCH_MEMORY_MMAPING_SYNCRONIZATION
    region_table *regionTable = instrument_args.region_table;
    if(regionTable->needsSync){
        printf("received request to sync from backend\n");
        synchronize_address_space(instrument_args.region_table, false);
    }

#endif

#ifdef DEBUG_INSTRUMENTATION
    printf("Entires in hash set: %d\n", *data->number_of_hash_set_entries);
    printf("Instrumented memcpy's: %d\n", data->memcpyArray.count);
#endif
    data->thread_local_head = get_global_head_value(ringBuffer);
    data->thread_local_tail = get_global_tail_value(ringBuffer);
    for(int i=0; i < data->memcpyArray.count; i++){
        char *start = data->memcpyArray.array[i].start;
        char *end = data->memcpyArray.array[i].end;
        int distance = end - start;
        for (int j=0; j < distance; j=j+PAYLOAD_SIZE){
#ifdef LOG_TO_RING_BUFFER
            ring_buffer_enqueue_redo_log_entry(data->pmem_buffer,
                                               start+j, &data->thread_local_head,
                                               &data->thread_local_tail);
#endif
        }
        ring_buffer_commit_enqueued_entries(data->pmem_buffer, &data->thread_local_head, &data->thread_local_tail);

    }

    collect_payload_and_store_to_pmem(data, true); // Change to no commit
    *data->number_of_hash_set_entries = 0;
    overflowBuffer.size = 0;
    data->memcpyArray.count = 0;
#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR, "INSTRUMENTATION STOPPED\n");
#endif
}



static void reset_hash_set(){

    void *drcontext = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    data->combiner = 0;
#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR, "RESETTING HASH SET Length: %lu\n", data->hash_set_metadata.size*8);
#endif
    bzero(data->hash_set_entries, data->hash_set_metadata.size*8);
}

void free_mem_info(void *p)
{
    dr_global_free(p, sizeof(dr_mem_info_t));
}

static bool should_replace(const dr_mem_info_t *info) {
    // Code copied from https://github.com/persimmon-project/persimmon
    app_pc base = info->base_pc;
    if (dr_memory_is_dr_internal(base)) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "[should_replace] skipping internal memory:\t%p\n", base);
#endif
        return false;
    }

    if (dr_memory_is_in_client(base)) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "[should_replace] skipping client memory:\t%p\n", base);
#endif
        return false;
    }

    if (info->type == DR_MEMTYPE_FREE) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "[should_replace] skipping MEMTYPE_FREE:\t%p\n", base);
#endif
        return false;
    }
    if (!(info->prot & DR_MEMPROT_WRITE)) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "[should_replace] skipping no-write memory:\t%p\n", base);
#endif
        return false;
    }
    if (info->prot & DR_MEMPROT_VDSO) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "[should_replace] skipping VDSO:\t%p\n", base);
#endif
        return false;
    }
    if (info->prot & DR_MEMPROT_STACK) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "[should_replace] skipping stack:\t%p\n", base);
#endif
        return false;
    }

    if (info->prot & DR_MEMPROT_SHARED) {
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "[should_replace] skipping shared page:\t%p\n", base);
#endif
        return false;
    }
    DR_ASSERT(!(info->prot & DR_MEMPROT_PRETEND_WRITE));
    DR_ASSERT(info->prot & DR_MEMPROT_READ);
    return true;
}

static void synchronize_address_space(region_table *regionTable, bool initial){
    // Part of the code in this method is copied from https://github.com/persimmon-project/persimmon
    drvector_t to_replace; // Vector (of `dr_mem_info_t *`) of memory regions to replace.
    {
        bool success = drvector_init(&to_replace, /* initial_capacity */ 10, /* synch */ false,
        free_mem_info);
        DR_ASSERT(success);
    }

    dr_mem_info_t info;
    app_pc pc = NULL;
    while ((uintptr_t) pc < 0xffffffffffffffff && dr_query_memory_ex(pc, &info)) {
        if (should_replace(&info)) {

            if (initial) {

                void *info_mem = dr_global_alloc(sizeof(dr_mem_info_t));
                memcpy(info_mem, &info, sizeof(dr_mem_info_t));
                dr_fprintf(STDERR, "Replacing region %p with size : %d\n", info.base_pc, info.size);
#ifdef DEBUG_INSTRUMENTATION
                dr_fprintf(STDERR, "Replacing region %p with size : %d\n", info.base_pc, info.size);
#endif
			bool success = drvector_append(&to_replace, info_mem);
                        DR_ASSERT(success);

            } else{
                    bool missing_from_table = true;

                        for(uint64_t i=0; i < regionTable->number_of_entries; i++){
                            byte *start = regionTable->regionTable[i].address;
                            if(pc == start){
                                missing_from_table = false;
                                break;
                            }
                        }
                    if(missing_from_table){
                        regionTable->newEntries[regionTable->number_of_new_entries].address = pc;
                        regionTable->newEntries[regionTable->number_of_new_entries].mmap_size = info.size;
                        regionTable->number_of_new_entries++;
                    }
            }

        }
        pc = info.base_pc + info.size;
    }

    printf("sent ready signal to consumer\n");
    regionTable->needsSync = false;

    if(initial) {
        // Now iterate through the memory regions to replace, and replace them.
        for (uint i = 0; i < to_replace.entries; i++) {
            dr_mem_info_t *infop = (dr_mem_info_t *) (to_replace.array[i]);

            uint dr_prot = infop->prot;
            int prot = 0;
            if (dr_prot & DR_MEMPROT_READ) {
                prot |= PROT_READ;
            }
            if (dr_prot & DR_MEMPROT_WRITE) {
                prot |= PROT_WRITE;
            }
            if (dr_prot & DR_MEMPROT_EXEC) {
                prot |= PROT_EXEC;
            }


            int fd = create_pmem_file(infop->size);
            printf("Requesting to mmap : %p with size: %lu\n", infop->base_pc, infop->size);
#ifdef DEBUG_INSTRUMENTATION
            printf("Requesting to mmap : %p with size: %lu\n", infop->base_pc, infop->size);
#endif
	    void *destination = mmap(NULL, infop->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	     if (destination == MAP_FAILED) {
                 printf("MMAP FAILED\n");
		 exit(1);
	     }
printf("mmap scratch is ok\n");
            memcpy(destination, infop->base_pc, infop->size);
printf("memcpy is ok\n");
            pmem_persist(destination, infop->size);
printf("persist is ok\n");
            munmap(destination, infop->size);

printf("munmap scratch is ok\n");
            void *returnedMmapAddress = mmap(infop->base_pc, infop->size, infop->prot,
                                             MAP_FIXED | MAP_SHARED, fd, 0);

#ifdef DEBUG_INSTRUMENTATION
            dr_fprintf(STDERR, "Returned address: %p\n", returnedMmapAddress);
#endif
            dr_fprintf(STDERR, "Returned address: %p\n", returnedMmapAddress);
            region_table_add_region_record(instrument_args.region_table, returnedMmapAddress, infop->size);
        }
        region_table_commit(instrument_args.region_table);

        bool success = drvector_delete(&to_replace); // This frees all elements.
        DR_ASSERT(success);
    }
}
static void remap_address_space(region_table *regionTable){
#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR,"Calling a remap from dynamorio\n");
    dr_fprintf(STDERR,"Table content:\n");
#endif
    for(uint64_t i = 0; i < regionTable->number_of_entries; i++){
        region_table_entry *regionTableEntry = &regionTable->regionTable[i];
        if(regionTableEntry->mmap_size == 2621472){
            dr_fprintf(STDERR,"ERROR tried to remap ring buffer\n");
            exit(1);
        }
#ifdef DEBUG_INSTRUMENTATION
        printf("Entry 1: addr: %p, size: %lu, file_index: %lu\n", regionTableEntry->address, regionTableEntry->mmap_size, regionTableEntry->file_index);
#endif
    }

    for(uint64_t i = 0; i < regionTable->number_of_entries; i++){
        if(true) { 
            int prot = PROT_READ | PROT_WRITE;
            int flags = MAP_FIXED | MAP_SHARED_VALIDATE | MAP_SYNC;
#ifdef DEBUG_INSTRUMENTATION
            dr_fprintf(STDERR, "Generating file name\n");
#endif
            char fileName[MAX_FILE_NAME_SIZE];
            get_file_name_for_sequence_number(fileName, i);
#ifdef DEBUG_INSTRUMENTATION
            dr_fprintf(STDERR, "File name: %s\n", fileName);
#endif
            int fd = open(fileName, O_RDWR, 0666);
            if (fd == -1) {
                printf("ERROR OPENING FILE\n");
            }

            region_table_entry *regionTableEntry = &regionTable->regionTable[i];
#ifdef DEBUG_INSTRUMENTATION
            printf("Opening file: %s which corresponds to region %p with size: %lu\n", fileName,
                   regionTableEntry->address, regionTableEntry->mmap_size);
#endif
            void *returnedMmapAddress = mmap(regionTableEntry->address, regionTableEntry->mmap_size, prot,
                                             flags, fd, 0);
#ifdef DEBUG_INSTRUMENTATION
            printf("Returned address: %p\n", returnedMmapAddress);
#endif
            assert(returnedMmapAddress == regionTableEntry->address);
        }
    }
}


void copyDataFromPersistentMemory(){
    
#ifdef DEBUG_INSTRUMENTATION
    printf("copy_data_from_pmem start\n");
#endif
    region_table *regionTable = instrument_args.region_table;
    
    uint64_t number_of_entries = regionTable->number_of_entries;
    for(uint64_t i = 0; i < number_of_entries; i++){
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR,"Generating file name\n");
#endif
        char fileName[MAX_FILE_NAME_SIZE];
        get_file_name_for_sequence_number(fileName, i);
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "File name: %s\n", fileName);
#endif
        int fd = open(fileName, O_RDWR, 0666);
        if(fd == -1){
            printf("ERROR OPENING FILE\n");
        }
        region_table_entry *regionTableEntry = &regionTable->regionTable[i];
#ifdef DEBUG_INSTRUMENTATION
        printf("Opening file: %s which corresponds to region %p with size: %lu\n",fileName, regionTableEntry->address, regionTableEntry->mmap_size);
#endif
        if(true) {
            void *source = mmap(NULL, regionTableEntry->mmap_size, PROT_READ | PROT_WRITE,
                                MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
#ifdef DEBUG_INSTRUMENTATION
            printf("source map is ok\n");
#endif
            void *destination = regionTableEntry->address;

            void *returnedMmapAddress = mmap(destination, regionTableEntry->mmap_size, PROT_READ | PROT_WRITE,
                                             MAP_FIXED | MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);

            assert(returnedMmapAddress == destination);
#ifdef DEBUG_INSTRUMENTATION
            printf("destination map is ok: %p\n", returnedMmapAddress);
#endif
            memcpy(destination, source, regionTableEntry->mmap_size);
#ifdef DEBUG_INSTRUMENTATION
            printf("memcpy is okay\n");
#endif
            munmap(source, regionTableEntry->mmap_size);
#ifdef DEBUG_INSTRUMENTATION
            printf("munmap is okay\n");
#endif
        } else{
            dr_fprintf(STDERR, "DR recovery Skipping thread related region\n");
        }
    }
#ifdef DEBUG_INSTRUMENTATION
    printf("copy_data_from_pmem done\n");
#endif
}

void commit_rb_entries(){
#ifdef DEBUG_INSTRUMENTATION
    printf("Committing to RB\n");
#endif
    ring_buffer_commit_enqueued_entries(ringBuffer, &global_local_head, &global_local_tail);
}



DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_fprintf(STDERR,"starting initialisation 685\n");
    overflowBuffer.size = 0;
#ifdef COLLECT_REDO_LOG_METRICS
    totalMemcpy = 0;
    totalPersist = 0;
#endif
    /* We need 2 reg slots beyond drreg's eflags slots => 3 slots */
    drreg_options_t ops = { sizeof(ops), 3, false };
    /* Specify priority relative to other instrumentation operations: */
    drmgr_priority_t priority = { sizeof(priority), /* size of struct */
                                  "memtrace",       /* name of our operation */
                                  NULL, /* optional name of operation we should precede */
                                  NULL, /* optional name of operation we should follow */
                                  0 };  /* numeric priority */
    dr_set_client_name("DynamoRIO Sample Client 'memtrace'",
                       "http://dynamorio.org/issues");
    fc_writers = instrument_args.writers;
    if(instrument_args.state == BACK_END_INITIAL){
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR, "Attaching to consumer\n");
#endif
        synchronize_address_space(instrument_args.region_table, true);
        dr_app_stop_and_cleanup();
        //exit(0);
    }
    else if(instrument_args.state == BACK_END_RECOVERY) {
        assert(instrument_args.region_table != NULL);
        remap_address_space(instrument_args.region_table);
        dr_app_stop_and_cleanup();
    }else if(instrument_args.state == FRONT_END_INITIAL){
            dr_annotation_register_call("commit_rb_entries", commit_rb_entries, false, 0,
                                        DR_ANNOTATION_CALL_TYPE_FASTCALL);

            ringBuffer = instrument_args.ringBuffer;
            page_size = dr_page_size();
            drmgr_init();
            drutil_init();
            client_id = id;
            mutex = dr_mutex_create();
            dr_register_exit_event(event_exit);
            if (!drmgr_register_thread_init_event(event_thread_init) ||
                !drmgr_register_thread_exit_event(event_thread_exit) ||
                !drmgr_register_bb_app2app_event(event_bb_app2app, &priority) ||
                !drmgr_register_bb_instrumentation_event(event_bb_analysis, event_bb_insert,
                                                         &priority) ||
                drreg_init(&ops) != DRREG_SUCCESS || !drx_init()) {
                /* something is wrong: can't continue */
                DR_ASSERT(false);
                return;
            }

#ifndef USE_PER_BATCH_MEMORY_MMAPING_SYNCRONIZATION
            dr_register_filter_syscall_event(event_filter_syscall);
            if (!drmgr_register_pre_syscall_event(event_pre_syscall)) {
                DR_ASSERT(false);
            }
            if (!drmgr_register_post_syscall_event(event_post_syscall)) {
                DR_ASSERT(false);
            }
#endif
        printf("about to initialisese 746\n");
        } else{
ringBuffer = instrument_args.ringBuffer;
#ifdef DEBUG_INSTRUMENTATION
        dr_fprintf(STDERR,"DR main rb initialized: head %lu, tail %lu\n", ringBuffer->remote_head, ringBuffer->remote_tail);
#endif


    page_size = dr_page_size();
    drmgr_init();
    printf("about to initialisese 763\n");

        drwrap_init();
        assert(drmgr_register_module_load_event(module_load_event));

    drutil_init();
    client_id = id;
    mutex = dr_mutex_create();
    dr_register_exit_event(event_exit);
    if (!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_bb_app2app_event(event_bb_app2app, &priority) ||
        !drmgr_register_bb_instrumentation_event(event_bb_analysis, event_bb_insert,
                                                 &priority) ||
        drreg_init(&ops) != DRREG_SUCCESS || !drx_init()) {
        /* something is wrong: can't continue */
        DR_ASSERT(false);
        return;
    }

    dr_annotation_register_call("instrument_start", instrument_start, false, 0,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
    dr_annotation_register_call("instrument_stop", instrument_stop, false, 0,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
    dr_annotation_register_call("reset_hash_set", reset_hash_set, false, 0,
                                DR_ANNOTATION_CALL_TYPE_FASTCALL);
        dr_annotation_register_call("copyDataFromPersistentMemory", copyDataFromPersistentMemory, false, 0,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);

        dr_annotation_register_call("collect_mmaps", collect_mmaps, false, 0,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);

        dr_annotation_register_call("commit_mmaps", commit_mmaps, false, 0,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);
        dr_annotation_register_call("setCombinerToInitilise", setCombinerToInitilise, false, 0,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);


        dr_annotation_register_call("instrumented_memcpy", instrumented_memcpy, false, 0,
                                    DR_ANNOTATION_CALL_TYPE_FASTCALL);

        tls_index = drmgr_register_tls_field();

        DR_ASSERT(tls_index != -1);

    code_cache_init();

#ifndef USE_PER_BATCH_MEMORY_MMAPING_SYNCRONIZATION
    dr_register_filter_syscall_event(event_filter_syscall);
    if (!drmgr_register_pre_syscall_event(event_pre_syscall)) {
        DR_ASSERT(false);
    }
    if (!drmgr_register_post_syscall_event(event_post_syscall)) {
        DR_ASSERT(false);
    }
#endif
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'memtrace' initializing\n");
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client memtrace is running\n");
    }
#endif
    }
}

static void
event_exit()
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "  saw %llu memory references\n",
                      global_num_refs);
    DR_ASSERT(len > 0);

#endif /* SHOW_RESULTS */
    code_cache_exit();

    if (!drmgr_unregister_tls_field(tls_index) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_bb_insertion_event(event_bb_insert) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);

    dr_mutex_destroy(mutex);
    drutil_exit();
    drmgr_exit();
    drx_exit();
}

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS(x) /* nothing */
#endif

static void
event_thread_init(void *drcontext)
{
    per_thread_t *data;

    /* allocate thread private data */
    data = dr_thread_alloc(drcontext, sizeof(per_thread_t));
    drmgr_set_tls_field(drcontext, tls_index, data);

    data->pmem_buffer = ringBuffer;
    uint64_t addr = (uint64_t) data->pmem_buffer;
    assert(addr % 64 == 0);

    int hash_set_size = HASH_SET_SIZE;
    double resize_threshold = 1;
    data->number_of_hash_set_entries = dr_thread_alloc(drcontext, sizeof(int));
    data->hash_set_entries = dr_thread_alloc(drcontext,hash_set_size*8);
    data->hash_set_metadata.size = hash_set_size;
    data->hash_set_metadata.maxElementsBeforeResize = (int)(hash_set_size * resize_threshold);
    data->hash_set_metadata.numberOfEntries = 0;
    data->hash_set_metadata.numberOfDuplicates = 0;
    data->hash_set_metadata.numberOfCollisions = 0;
    data->hash_set_metadata.numberOfResizes = 0;
    data->lastSyscall.sysnum = 0; // The check only cares about mmap calls
    bzero(data->hash_set_entries, hash_set_size*8);
    data->num_refs = 0;
    data->combiner = 0;
    data->skip_mmap = 0;
    data->deferredMmapEntries.number_of_regions_in_this_batch = 0;
    data->hashSetBypassBuffer = &hashSetBypassBuffer;
#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR,"Thread is configured\n");
#endif
}



static void
event_thread_exit(void *drcontext)
{
    // TODO Implement correct exit
    per_thread_t *data;
    data = drmgr_get_tls_field(drcontext, tls_index);
    dr_mutex_lock(mutex);
    global_num_refs += data->num_refs;
    dr_mutex_unlock(mutex);

    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    if (!drutil_expand_rep_string(drcontext, bb)) {
        DR_ASSERT(false);
    }
    if (!drx_expand_scatter_gather(drcontext, bb, NULL)) {
        DR_ASSERT(false);
    }
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, void **user_data)
{
    instru_data_t *data = (instru_data_t *)dr_thread_alloc(drcontext, sizeof(*data));
    data->last_pc = NULL;
    *user_data = (void *)data;
    return DR_EMIT_DEFAULT;
}

/* event_bb_insert calls instrument_mem to instrument every
* application memory reference.
 */
static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *where,
                bool for_trace, bool translating, void *user_data)
{
    per_thread_t *threadData;

    threadData = drmgr_get_tls_field(drcontext, tls_index);

    int i;
    instru_data_t *data = (instru_data_t *)user_data;
    
    instr_t *instr_fetch = drmgr_orig_app_instr_for_fetch(drcontext);
    if (instr_fetch != NULL)
        data->last_pc = instr_get_app_pc(instr_fetch);
    app_pc last_pc = data->last_pc;
    if (drmgr_is_last_instr(drcontext, where))
        dr_thread_free(drcontext, data, sizeof(*data));

    instr_t *instr_operands = drmgr_orig_app_instr_for_operands(drcontext);
    if (instr_operands == NULL ||
        (!instr_writes_memory(instr_operands) && !instr_reads_memory(instr_operands)))
        return DR_EMIT_DEFAULT;
    DR_ASSERT(instr_is_app(instr_operands));
    DR_ASSERT(last_pc != NULL);

    if (instr_writes_memory(instr_operands)) {
        for (i = 0; i < instr_num_dsts(instr_operands); i++) {
            opnd_t ref = instr_get_dst(instr_operands, i);
            if (opnd_is_memory_reference(ref)) {
                if (opnd_is_base_disp(ref) && opnd_get_base(ref) == DR_REG_XSP) {
                    continue;
                }
                instrument_mem(drcontext, bb, where, last_pc, instr_operands, i, true, ref);
            }
        }
    }
    return DR_EMIT_DEFAULT;
}
#ifndef DR_PARAM_OUT
#    define DR_PARAM_OUT /* marks output param */
#endif
#ifndef DR_PARAM_INOUT
#    define DR_PARAM_INOUT /* marks input+output param */
#endif

#define MEMCPY_OVERWRITE
static void
wrap_pre(void *wrapcxt, DR_PARAM_OUT void **user_data)
//wrap_pre()
{
    void *dst = (void*)drwrap_get_arg(wrapcxt, 0);
    void *src = (void*)drwrap_get_arg(wrapcxt, 1);
    size_t size_of_copy = (int)drwrap_get_arg(wrapcxt, 2);
#ifdef DEBUG_INSTRUMENTATION
    dr_fprintf(STDERR,"memcpy arg 0: %p\n", dst);
    dr_fprintf(STDERR,"memcpy arg 1: %p\n", src);
    dr_fprintf(STDERR,"memcpy arg 0: %d\n", size_of_copy);
#endif
    void *drcontext  = dr_get_current_drcontext();
    per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_index);
    bool exectute_memcpy = false;
    if(data->combiner == 1){
        //_mm_sfence();
#ifdef MEMCPY_OVERWRITE
        // if(size_of_copy == 100001){
        data->memcpyArray.array[data->memcpyArray.count].start = dst;
        data->memcpyArray.array[data->memcpyArray.count].end = dst+size_of_copy;
        // TODO store in thread local buffer and increment counter.
        data->memcpyArray.count++;
        exectute_memcpy = true;
        memcpy(dst,src,size_of_copy);
        drwrap_set_arg(wrapcxt, 2, 0);
        //   }
#endif
    }
    *user_data = (void *)exectute_memcpy;

}

static void
wrap_post(void *wrapcxt, void *user_data)
{
}

static void
memtrace(void *drcontext)
{
}

static void
clean_call(void)
{
    void *drcontext = dr_get_current_drcontext();
    memtrace(drcontext);
}

static void
code_cache_init(void)
{
    void *drcontext;
    instrlist_t *ilist;
    instr_t *where;
    byte *end;

    drcontext = dr_get_current_drcontext();
    code_cache =
            dr_nonheap_alloc(page_size, DR_MEMPROT_READ | DR_MEMPROT_WRITE | DR_MEMPROT_EXEC);
    ilist = instrlist_create(drcontext);
    /* The lean procedure simply performs a clean call, and then jumps back
     * to the DR code cache.
     */
    where = INSTR_CREATE_jmp_ind(drcontext, opnd_create_reg(DR_REG_XCX));
    instrlist_meta_append(ilist, where);
    /* clean call */
    dr_insert_clean_call(drcontext, ilist, where, (void *)clean_call, false, 0);
    /* Encodes the instructions into memory and then cleans up. */
    end = instrlist_encode(drcontext, ilist, code_cache, false);
    DR_ASSERT((size_t)(end - code_cache) < page_size);
    instrlist_clear_and_destroy(drcontext, ilist);
    /* set the memory as just +rx now */
    dr_memory_protect(code_cache, page_size, DR_MEMPROT_READ | DR_MEMPROT_EXEC);
}

static void
code_cache_exit(void)
{
    dr_nonheap_free(code_cache, page_size);
}

void print_combiner_value(int value){
    per_thread_t *data;

    data = drmgr_get_tls_field(dr_get_current_drcontext(), tls_index);
}

void print_when_logging(uintptr_t addr){
    per_thread_t *data;

    data = drmgr_get_tls_field(dr_get_current_drcontext(), tls_index);
    char payload[32];
}

void print_progress_main(void){
    printf("in the main\n");
}

static void
module_load_event(void *drcontext, module_data_t *mod, bool loaded)
{
    app_pc towrap = (app_pc)dr_get_proc_address(mod->handle, "memcpy");
    if (towrap != NULL) {
        bool ok =
                drwrap_wrap(towrap, wrap_pre, wrap_post);
        assert(ok);
    }
}

static void
instrument_mem(void *drcontext, instrlist_t *ilist, instr_t *where, app_pc pc,
               instr_t *memref_instr, int pos, bool write, opnd_t original_opnd)
{
    instr_t *instr;
    assembly_args assemblyArgs;
    assemblyArgs.drcontext = drcontext;
    assemblyArgs.ilist = ilist;
    assemblyArgs.where = where;
    assemblyArgs.memref_instr = memref_instr;
    assemblyArgs.pos = pos;
    assemblyArgs.instr = instr;
    uint size = drutil_opnd_mem_size_in_bytes(original_opnd, instr);
    assemblyArgs.write_size = size;

    drvector_t allowed;
    per_thread_t *data;

    data = drmgr_get_tls_field(drcontext, tls_index);
    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, DR_REG_XCX, true);
    if (drreg_reserve_register(drcontext, ilist, where, NULL, &assemblyArgs.reg_storing_hash_set_pointer) !=
        DRREG_SUCCESS){
        DR_ASSERT(false);
    }
    drvector_delete(&allowed);


    drreg_init_and_fill_vector(&allowed, false);
    drreg_set_vector_entry(&allowed, DR_REG_RSP, true);
    if (drreg_reserve_register(drcontext, ilist, where, NULL, &assemblyArgs.rsp) !=
        DRREG_SUCCESS){
        DR_ASSERT(false);
    }
    drvector_delete(&allowed);

    if( drreg_reserve_register(drcontext, ilist, where, NULL, &assemblyArgs.rax) != DRREG_SUCCESS) {
        DR_ASSERT(false);
    }

    if(drreg_reserve_register(drcontext, ilist, where, NULL, &assemblyArgs.reg_storing_destination) != DRREG_SUCCESS){
        DR_ASSERT(false);
    }

    if( drreg_reserve_register(drcontext, ilist, where, NULL, &assemblyArgs.range) != DRREG_SUCCESS) {
        DR_ASSERT(false);
    }

    if( drreg_reserve_register(drcontext, ilist, where, NULL, &assemblyArgs.value_at_index) != DRREG_SUCCESS) {
        DR_ASSERT(false);
    }

    if( drreg_reserve_register(drcontext, ilist, where, NULL, &assemblyArgs.counter_pointer_reg) != DRREG_SUCCESS) {
        DR_ASSERT(false);
    }

    drmgr_insert_read_tls_field(drcontext, tls_index, ilist, where, assemblyArgs.reg_storing_hash_set_pointer);

    if (drreg_reserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "failed to reserve aflags");
    }

#ifdef USE_DEDUPLICATION_BUFFER
    store_address_in_hash_set(&assemblyArgs);
#else
    store_address_in_buffer(&assemblyArgs);
#endif
    if (drreg_unreserve_aflags(drcontext, ilist, where) != DRREG_SUCCESS) {
        DR_ASSERT_MSG(false, "failed to unreserve aflags");
    }

    if (drreg_unreserve_register(drcontext, ilist, where, assemblyArgs.reg_storing_destination) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, assemblyArgs.reg_storing_hash_set_pointer) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, assemblyArgs.range) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, assemblyArgs.rax) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, assemblyArgs.value_at_index) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, assemblyArgs.rsp) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, assemblyArgs.counter_pointer_reg) != DRREG_SUCCESS)
        DR_ASSERT(false);
}
