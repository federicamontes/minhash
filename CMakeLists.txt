cmake_minimum_required(VERSION 3.16)
project(concurrent_minhash C)


enable_testing()


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(LOCKS "Enable lock-based sketch implementation" OFF)
option(RW_LOCKS "Enable read-write locks for sketch implementation" OFF)
option(FCDS "Enable fcds sketch implementation" OFF)
option(CONC_MINHASH "Enable CONCURRENT MINHASH sketch implementation" OFF)

add_compile_options(-Wall -Wextra -pedantic -g)

# Enable pthread support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(LOCKS AND RW_LOCKS)
    message(FATAL_ERROR "LOCKS and RW_LOCKS options are mutually exclusive. Please enable only one.")
endif()


if(LOCKS)
    add_compile_definitions(LOCKS)
    message(STATUS "Using LOCKS-based sketch implementation.")
elseif(RW_LOCKS)
    add_compile_definitions(RW_LOCKS)
    message(STATUS "Using RW_LOCKS-based sketch implementation.")
elseif(FCDS)
    add_compile_definitions(FCDS)
    message(STATUS "Using FCDS sketch implementation.")
else()
    add_compile_definitions(CONC_MINHASH)
    set(CONC_MINHASH ON CACHE BOOL "Enable CONCURRENT MINHASH sketch implementation" FORCE)
    message(STATUS "Using default CONCURRENT MINHASH sketch implementation. ${CONC_MINHASH}")
endif()


include_directories(include)

add_subdirectory(src)
add_subdirectory(test)
