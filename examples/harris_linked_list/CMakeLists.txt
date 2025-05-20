cmake_minimum_required(VERSION 3.10)
project(queue_project LANGUAGES CXX)

# Compiler flags
add_compile_options(
    -fPIC
    -Wall
    -Wextra
    -Wno-uninitialized
    -Wno-unused-parameter
    -Wl,-zdefs,
    -O3
)




# Use modern C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link jemalloc globally (can also be target-specific if preferred)
link_libraries(jemalloc)

# Build the executable from C++ source
add_executable(harris_list harris_list.cpp)


target_include_directories(harris_list PUBLIC /home/se00598/home/se00598/ssmem/include)
#target_link_directories(harris_list PUBLIC "/home/se00598/home/se00598/ssmem")
#target_link_libraries(harris_list PRIVATE ssmem)
target_link_libraries(harris_list PRIVATE /home/se00598/home/se00598/ssmem/libssmem.a)

# Link against mangosteen
target_link_libraries(harris_list PRIVATE mangosteen)

# Add include and library directories
target_include_directories(harris_list PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(harris_list PUBLIC ${CMAKE_SOURCE_DIR}/include/magosteen_build)
