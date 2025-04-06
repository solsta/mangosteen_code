cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_C_STANDARD 11)

add_executable(allocator_test transient_allocator.c)
