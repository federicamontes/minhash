cmake_minimum_required(VERSION 3.16)
project(concurrent_minhash C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(LOCKS "Enable lock-based sketch implementation" OFF)
option(RW_LOCKS "Enable read-write locks for sketch implementation" OFF)
option(FCDS "Enable fcds sketch implementation" OFF)

add_compile_options(-Wall -Wextra -pedantic -g)

if(LOCKS AND RW_LOCKS)
    message(FATAL_ERROR "LOCKS and RW_LOCKS options are mutually exclusive. Please enable only one.")
endif()

# Enable pthread support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(LOCKS)
    add_compile_definitions(LOCKS)
endif()

if(RW_LOCKS)
    add_compile_definitions(RW_LOCKS)
endif()

if(FCDS)
    add_compile_definitions(FCDS)
endif()


include_directories(include)

add_subdirectory(src)
add_subdirectory(test)
