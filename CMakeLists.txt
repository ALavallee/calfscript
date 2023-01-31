cmake_minimum_required(VERSION 3.23)
project(calfscript C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS_RELEASE "-O3")

add_executable(calfscript main.c calf/calf.h calf/calf.c)
