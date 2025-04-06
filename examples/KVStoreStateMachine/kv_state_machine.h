//
// Created by se00598 on 11/05/24.
//
#include "stdlib.h"
#ifndef MANGOSTEEN_INSTRUMENTATION_KV_STATE_MACHINE_H
#define MANGOSTEEN_INSTRUMENTATION_KV_STATE_MACHINE_H
#define STORE_SZ 65536
#define VAL_SZ 128
#define RESP_SZ 3

/* Existing application data types */
typedef struct kvCmd {
    enum {PUT, GET} type;
    signed int index;
    char value[VAL_SZ];
} kvCmd;

// App specific client context definition.
typedef struct kvClient{
    int id; // e.g. from connection fd
    kvCmd* currCmd;
    char response[RESP_SZ];
} kvClient;

// This should be in mangosteen header file?
typedef struct mangoResponse {
    void* response;
    size_t size;
} mangoResponse;
#endif //MANGOSTEEN_INSTRUMENTATION_KV_STATE_MACHINE_H

