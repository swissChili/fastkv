cmake_minimum_required(VERSION 3.13)
project(fastkv C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_STATIC_LIBRARY_PREFIX "")

option(NO_CONDITIONS "Disble condition parsing" OFF)

if(NO_CONDITIONS)
	add_compile_definitions(FASTKV_NO_CONDITIONS)
endif()

add_library(libfastkv fastkv.h fastkv.c)
add_executable(fastkv main.c)
target_link_libraries(fastkv libfastkv)
