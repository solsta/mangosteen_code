#ifndef _DYNAMORIO_ANNOTATIONS_H_
#define _DYNAMORIO_ANNOTATIONS_H_ 1

#include "/home/vagrant/dynamorio/core/lib/dr_annotations_asm.h"
//#include "../ring_buffer/ring_buffer.h"

/* To simplify project configuration, this pragma excludes the file from GCC warnings. */
#ifdef __GNUC__
# pragma GCC system_header
#endif

#define INSTRUMENT_START() \
    DR_ANNOTATION(instrument_start)

#define INSTRUMENT_STOP() \
    DR_ANNOTATION(instrument_stop)


#ifdef __cplusplus
extern "C" {
#endif

//DR_DECLARE_ANNOTATION(void, instrument_start, (void));
__attribute__((noinline, visibility("default")))void instrument_start(void)__attribute__((weak));

//DR_DECLARE_ANNOTATION(void, instrument_stop, (void));
__attribute__((noinline, visibility("default")))void instrument_stop(void)__attribute__((weak));
//__attribute__((noinline, visibility("default")))void commit_mmaps(int count, ring_buffer_map_entry *entries)__attribute__((weak));

//DR_DECLARE_ANNOTATION(void, reset_hash_set, (void));
__attribute__((noinline, visibility("default")))void reset_hash_set(void)__attribute__((weak));

__attribute__((noinline, visibility("default")))void copyDataFromPersistentMemory(void)__attribute__((weak));

__attribute__((noinline, visibility("default")))void commit_rb_entries(void)__attribute__((weak));

__attribute__((noinline, visibility("default")))void setCombinerToInitilise(void)__attribute__((weak));
//__attribute__((noinline, visibility("default")))void collect_mmaps(deferred_mmap_entries *destination)__attribute__((weak));

//DR_DEFINE_ANNOTATION(void, instrumented_memcpy,(void *src, void *dest, size_t size), )
__attribute__((noinline, visibility("default")))void instrumented_memcpy(void *dest, void *src, size_t size)__attribute__((weak));



__attribute__((noinline, visibility("default")))void instrument_stop_collection(void)__attribute__((weak));
__attribute__((noinline, visibility("default")))void instrument_complete_combiner_procedure(int *workerThreadIds, int numberOfWorkers)__attribute__((weak));


#ifdef __cplusplus
}
#endif

#endif
