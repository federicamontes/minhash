cmake_minimum_required(VERSION 3.16)
project(concurrent_minhash C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(LOCKS "Enable lock-based sketch implementation" OFF)
option(RW_LOCKS "Enable read-write locks for sketch implementation" OFF)

add_compile_options(-Wall -Wextra -pedantic -g)

if(LOCKS AND RW_LOCKS)
    message(FATAL_ERROR "LOCKS and RW_LOCKS options are mutually exclusive. Please enable only one.")
endif()

if(LOCKS)
    add_compile_definitions(LOCKS)
	add_compile_options(-pthread)
endif()

if(RW_LOCKS)
    add_compile_definitions(RW_LOCKS)
	add_compile_options(-pthread)
endif()


include_directories(include)

add_subdirectory(src)
add_subdirectory(test)
