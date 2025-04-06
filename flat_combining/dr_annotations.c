#include "dr_annotations.h"

DR_DEFINE_ANNOTATION(void, instrument_start,(void), )
DR_DEFINE_ANNOTATION(void, instrument_stop,(void), )
//DR_DEFINE_ANNOTATION(void, commit_mmaps,(int count, ring_buffer_map_entry *entries), )
DR_DEFINE_ANNOTATION(void, reset_hash_set,(void), )
DR_DEFINE_ANNOTATION(void, copyDataFromPersistentMemory,(void), )
DR_DEFINE_ANNOTATION(void, commit_rb_entries,(void), )
//DR_DEFINE_ANNOTATION(void, collect_mmaps,(deferred_mmap_entries *destination), )
DR_DEFINE_ANNOTATION(void, setCombinerToInitilise,(void), )
DR_DEFINE_ANNOTATION(void, instrumented_memcpy,(void *dest, void *src, size_t size), )