
link_libraries(jemalloc)
add_executable(queue queue.c)

target_link_libraries(queue mangosteen)
target_link_directories(queue PUBLIC ${CMAKE_SOURCE_DIR}/include/magosteen_build)
target_include_directories(queue PUBLIC ${CMAKE_SOURCE_DIR}/include)
