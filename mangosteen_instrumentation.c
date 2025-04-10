//
// Created by se00598 on 20/07/23.
//
#define _GNU_SOURCE

#include "jemalloc/jemalloc.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include "ring_buffer/ring_buffer.h"
#include "flat_combining/flat_combining.h"
#include "region_table/region_table.h"
#include "flat_combining/writer_preference_spin_lock.h"
#include "flat_combining/dr_annotations.h"
#include <criu/criu.h>
#include "back-end/back_end.h"
#include "front-end/front_end.h"

char *front_end_dir = "/home/vagrant/criu_service/initial_snapshot";

void take_initial_checkpoint(char *path) {
    criu_init_opts();
    if (criu_set_service_address("/home/vagrant/criu_service/criu_service.socket")) {
        printf("Criu setup failed");
    }
    int pid;
    pid = getpid();
    criu_set_pid(pid);

    int fd = open(path, O_DIRECTORY);
    criu_set_images_dir_fd(fd);
    criu_set_log_level(4);
    criu_set_leave_running(true);
    criu_set_log_file("restore.log");
    criu_set_file_locks(true);
    criu_set_shell_job(true);
    criu_set_ext_sharing(true);
    criu_set_evasive_devices(true);

    int res = criu_dump();
    if (res < 0) {
        printf("Criu dump failed\n");
        exit(1);
    }

}

bool new_process(region_table *regionTable) {
    return regionTable->number_of_entries == 0;
}

void configure_common_args(region_table *regionTable, ring_buffer *ringBuffer) {
    instrument_args.region_table = regionTable;
    instrument_args.ringBuffer = ringBuffer;
    instrument_args.writers = getWritersPointer();
}

void launch_mangosteen(bool rpc_mode) {

#ifndef SKIP_CHECKPOINT
    take_initial_checkpoint(front_end_dir);
#endif

    ring_buffer *ringBuffer = ring_buffer_create_and_initialise(ringBuffer);
    region_table *regionTable = region_table_create_or_open(ringBuffer);
    configure_common_args(regionTable, ringBuffer);

    bool isNewProcess = new_process(regionTable);

#ifndef RUN_WITHOUT_BACK_END
    if (fork() == 0) {
        start_back_end(isNewProcess);
	//printf("hello from fork\n");
	//exit(1);
    } else {
#endif
        start_front_end(rpc_mode, isNewProcess);
#ifndef RUN_WITHOUT_BACK_END
    }
#endif
}

void blank_method(){}

bool imangosteen_initialized = false;

void initialise_mangosteen(mangosteen_args *mangosteenArgs) {

	    printf("called init mangosteen \n");
    if(imangosteen_initialized ){
	    printf("Initializing thread\n");
	    //exit(1);
	    mangosteen_initialize_thread();
    } else {

	    printf("Initializing main\n");
	    //exit(1);
    allocateFlatCombiningdMemory();
    assert(mangosteenArgs!=NULL);
    imangosteen_initialized = true;

    // The 4 methods below need to be defined for each mode,
    // but they only need to be implemented for the RPC mode.
    enableCombinerCallBack = &blank_method;
    disableCombinerCallBack = &blank_method;
    resetAllocatorCallBack = &blank_method;
    setPerThreadLocalThreadDataCallBack = &blank_method;


    if(mangosteenArgs->mode == SINGLE_THREAD){
        launch_mangosteen(false);
    } else if(mangosteenArgs->mode == MULTI_THREAD){
        assert(mangosteenArgs->isReadOnly!=NULL);
        assert(mangosteenArgs->processRequest!=NULL);
        isReadOnlyCallBack = mangosteenArgs->isReadOnly;
        processRequestCallBack = mangosteenArgs->processRequest;
        launch_mangosteen(false);
    } else if (mangosteenArgs->mode == RPC){
        // TODO ADD assert statements
        isReadOnlyCallBack = mangosteenArgs->isReadOnly;
        loadServiceRequestCallBack = mangosteenArgs->loadServiceRequest;
        processRequestCallBack = mangosteenArgs->processRequest;
        allocateThreadLocalServiceRequestStructCallBack = mangosteenArgs->allocateThreadLocalServiceRequestStruct;
        freeThreadLocalServiceRequestStructCallBack = mangosteenArgs->freeThreadLocalServiceRequestStruct;
        getResponseInfoCallBack = mangosteenArgs->getResponseInfo;
        enableCombinerCallBack = mangosteenArgs->enableCombiner;
        disableCombinerCallBack = mangosteenArgs->disableCombiner;
        resetAllocatorCallBack = mangosteenArgs->resetAllocator;
        setPerThreadLocalThreadDataCallBack = mangosteenArgs->setPerThreadLocalThreadData;
        launch_mangosteen(true);
    }
    }
}
