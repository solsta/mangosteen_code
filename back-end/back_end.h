//
// Created by se00598 on 22/02/24.
//

#ifndef MANGOSTEEN_INSTRUMENTATION_BACK_END_H
#define MANGOSTEEN_INSTRUMENTATION_BACK_END_H
#include "../ring_buffer/ring_buffer.h"
#include "../region_table/region_table.h"
#include "../flat_combining/writer_preference_spin_lock.h"

_Noreturn void run_consumer(ring_buffer *ringBuffer, region_table *regionTable);
void start_back_end(bool isNewProcess);
#endif //MANGOSTEEN_INSTRUMENTATION_BACK_END_H
