cmake_minimum_required(VERSION 3.10)
project(queue_project LANGUAGES CXX)

# Compiler flags
add_compile_options(
    -fPIC
    -Wall
    -Wextra
    -Wno-uninitialized
    -Wno-unused-parameter
    -Wl,-zdefs
    -O3
)

# Use modern C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link jemalloc globally (can also be target-specific if preferred)
link_libraries(jemalloc)

# Build the executable from C++ source
add_executable(ms_lock_queue ms_lock_queue.cpp)

# Link against mangosteen
target_link_libraries(ms_lock_queue PRIVATE mangosteen)

# Add include and library directories
target_include_directories(ms_lock_queue PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(ms_lock_queue PUBLIC ${CMAKE_SOURCE_DIR}/include/magosteen_build)
