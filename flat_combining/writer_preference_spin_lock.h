//
// Created by se00598 on 07/03/23.
//

#ifndef SPIN_LOCK_WRITER_PREFERENCE_SPIN_LOCK_H
#define SPIN_LOCK_WRITER_PREFERENCE_SPIN_LOCK_H
#include <stdbool.h>
#include <stdatomic.h>
#include <unistd.h>
//#include <pthread.h>


//#define NUMBER_OF_THREADS 64
#define READING 0
#define NOT_READING 1

// Pause to prevent excess processor bus usage
#if defined( __sparc )
#define Pause() __asm__ __volatile__ ( "rd %ccr,%g0" )
#elif defined( __i386 ) || defined( __x86_64 )
#define Pause() __asm__ __volatile__ ( "pause" : : : )
#endif

/*? Should we use bitmask */
//_Atomic volatile bool readers[NUMBER_OF_THREADS];

_Atomic int *readers_set;//[NUMBER_OF_THREADS];

bool readLock(int thread_id);
bool lockWriter();
void readUnlock(int thread_id);

bool activeReadersSetIsEmpty();
bool tryWriteLock();
bool lockIsFree();
void writeUnlock();
_Atomic int *getWritersPointer();
#endif //SPIN_LOCK_WRITER_PREFERENCE_SPIN_LOCK_H
