#cmake_minimum_required(VERSION 3.16.3)
#project(mangosteen_local_example C)
#add_compile_options(-fPIC -Wall -Wextra -Wno-uninitialized -Wno-unused-parameter -Wl,-zdefs -O1)

#set(CMAKE_C_STANDARD 11)
link_libraries(jemalloc)
add_executable(local_state_machine local_state_machine.c)

target_link_libraries(local_state_machine mangosteen)
target_link_directories(local_state_machine PUBLIC ${CMAKE_SOURCE_DIR}/include/magosteen_build)
target_include_directories(local_state_machine PUBLIC ${CMAKE_SOURCE_DIR}/include)