cmake_minimum_required(VERSION 3.16)
project(fastkv C)

set(CMAKE_C_STANDARD 11)

add_executable(fastkv main.c fastkv.h fastkv.c)
