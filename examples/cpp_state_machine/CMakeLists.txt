#cmake_minimum_required(VERSION 3.16.3)
#project(mangosteen_cpp_example CPP)
#add_compile_options(-fPIC -Wall -Wextra -Wno-uninitialized -Wno-unused-parameter -Wl,-zdefs -O1)

link_libraries(jemalloc)
add_executable(cpp_state_machine state_machine.cpp)

#target_include_directories(cpp_state_machine PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(cpp_state_machine PUBLIC ${CMAKE_SOURCE_DIR}/)
target_link_libraries(cpp_state_machine libmangosteen.so)

