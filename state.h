
#define FRONT_END_INITIAL 0
#define FRONT_END_RECOVERY 1
#define FRONT_END_INITIAL_SECOND_PART 4
#define BACK_END_INITIAL 2
#define BACK_END_RECOVERY 3
#define INSTRUMENTATION_DEFAULT_STATE 10

typedef struct {
    int state;
    void *region_table;
    void *ringBuffer;
    _Atomic int *writers;
} instrument_args_t;

extern instrument_args_t instrument_args;