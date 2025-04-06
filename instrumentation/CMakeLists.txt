add_library(frontend STATIC instrument_front_end.c redo_entry_collector.c ../ring_buffer/ring_buffer.c instrumenation_common.h)
add_compile_options(-O3)
configure_DynamoRIO_static(frontend)
use_DynamoRIO_extension(frontend drreg_static)
use_DynamoRIO_extension(frontend drmgr_static)
use_DynamoRIO_extension(frontend drutil_static)
use_DynamoRIO_extension(frontend drwrap_static)
use_DynamoRIO_extension(frontend drx_static)
#set(DYNAMORIO_OPTIONS thread_private)
target_link_libraries(frontend pmem rt)
target_compile_options(frontend PRIVATE  -mavx -mclflushopt -mclwb)
#-fvisibility=hidden
#-Wall -Wextra -Wno-uninitialized -Wno-unused-parameter -Wl,-zdefs