//
// Created by se00598 on 23/02/24.
//

#ifndef MANGOSTEEN_INSTRUMENTATION_REDO_ENTRY_COLLECTOR_H
#define MANGOSTEEN_INSTRUMENTATION_REDO_ENTRY_COLLECTOR_H
#include "dr_api.h"
#include "drutil.h"
#include "instrumenation_common.h"


void store_address_in_hash_set(assembly_args *assemblyArgs);
void store_address_in_buffer(assembly_args *assemblyArgs);
#endif //MANGOSTEEN_INSTRUMENTATION_REDO_ENTRY_COLLECTOR_H
