//
// Created by se00598 on 07/03/23.
//
#include "flat_combining.h"

#include "writer_preference_spin_lock.h"
#include <x86intrin.h>


_Atomic int writers = 0;

bool noActiveWriter(){return writers == 0;}
bool lockIsFree(){return noActiveWriter();}

_Atomic int *getWritersPointer(){
    return &writers;
}

void markReaderAsActive(int thread_id){
    int padding_size = 128/sizeof(int);
    readers_set[thread_id*padding_size] = READING;
    _mm_mfence();
}
void markReaderAsInactive(int thread_id){
    int padding_size = 128/sizeof(int);
    readers_set[thread_id*padding_size] = NOT_READING;
}

bool activeReadersSetIsEmpty(){
    int padding_size = 128/sizeof(int);
    for(int i=0; i < NUMBER_OF_THREADS; i++){
        if(readers_set[i*padding_size] == READING){return false;}
    }
    //printf("Active reader set not empty!\n");
    return true;
}

bool lockWriter(){
    _Atomic int expected = 0;
    _Atomic int desired = 1;
    return __atomic_compare_exchange(&writers, &expected, &desired, false,memory_order_seq_cst,memory_order_seq_cst);
}

void releaseWriter(){
    writers = 0;
}

bool tryWriteLock(){
    if(lockIsFree()){
        if(lockWriter() == true){
            //pthread_yield();
            // This is what gives writer preference.
            while(!activeReadersSetIsEmpty()){
                Pause();
                //pthread_yield();
            }
            return true;
        }
    }
    //pthread_yield();
    return false;
}

void writeUnlock(){
    releaseWriter();
}

bool readLock(int thread_id){
    while(true){
        markReaderAsActive(thread_id);
        if(lockIsFree()){return true;}
        markReaderAsInactive(thread_id);
        while(!lockIsFree()){
            Pause();
            //pthread_yield();
        }
    }
}

void readUnlock(int thread_id) {
    markReaderAsInactive(thread_id);
}
