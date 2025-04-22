cmake_minimum_required(VERSION 3.16.3)
project(mangosteen_instrumentation C CXX)

add_compile_options(-fPIC -Wall -Wextra -Wno-uninitialized -Wno-unused-parameter -Wl,-zdefs -O3)

set(CMAKE_C_STANDARD 11)
set(DynamoRIO_DIR "/home/vagrant/dynamorio/build/cmake")
include_directories(${DynamoRIO_INCLUDE_DIRS})
include_directories(front-end)

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

add_subdirectory(instrumentation)
add_subdirectory(examples/mangosteen_local)
add_subdirectory(examples/queue)
add_subdirectory(examples/ms_lock_based_queue)
add_subdirectory(examples/mangosteen_local_multi_thread)
add_subdirectory(examples/cpp_state_machine)
add_subdirectory(examples/mangosteen_rpc/rpc_state_machine)
add_subdirectory(examples/KVStoreStateMachine)
add_library(mangosteen SHARED mangosteen_instrumentation.c mangosteen_instrumentation.h ring_buffer/ring_buffer.c hash_set/hash_set.c hash_set/hash_set.h flat_combining/flat_combining.c flat_combining/flat_combining.h flat_combining/writer_preference_spin_lock.c flat_combining/common.h flat_combining/dr_annotations.c flat_combining/dr_annotations.h region_table/region_table.c region_table/region_table.h configuration.h netutils/netutils.c back-end/back_end.c front-end/front_end.c front-end/front_end.h )

target_link_libraries(mangosteen pmem frontend pthread criu dl) #
target_compile_options(mangosteen PRIVATE -DUSE_VISIBILITY_ATTRIBUTES -mavx -mclflushopt -mclwb)

