cmake_minimum_required(VERSION 3.16)
project(fastkv C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_STATIC_LIBRARY_PREFIX "")

add_library(libfastkv fastkv.h fastkv.c)
add_executable(fastkv main.c)
target_link_libraries(fastkv libfastkv)
