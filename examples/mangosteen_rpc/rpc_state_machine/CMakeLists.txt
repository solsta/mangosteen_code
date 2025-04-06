#cmake_minimum_required(VERSION 3.16.3)
#project(mangosteen_cpp_example CPP)
#add_compile_options(-fPIC -Wall -Wextra -Wno-uninitialized -Wno-unused-parameter -Wl,-zdefs -O1)
add_compile_options(-O1)
link_libraries(pthread dl jemalloc)#jemalloc
add_executable(state_machine_rpc state_machine_rpc.cpp)
add_executable(rpc_client client_rpc.cpp)
target_link_libraries(state_machine_rpc dl)
#target_link_libraries(linked_list_state_machine "/home/se00598/home/se00598/persimmon/redis/deps/jemalloc4/lib/libjemalloc.a")

#target_include_directories(cpp_state_machine PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_directories(state_machine_rpc PUBLIC ${CMAKE_SOURCE_DIR}/)
target_link_libraries(state_machine_rpc libmangosteen.so)

