cmake_minimum_required(VERSION 3.16)
project(concurrent_minhash C)


enable_testing()


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(LOCKS "Enable lock-based sketch implementation" OFF)
option(RW_LOCKS "Enable read-write locks for sketch implementation" OFF)
option(FCDS "Enable fcds sketch implementation" OFF)
option(CONC_MINHASH "Enable CONCURRENT MINHASH sketch implementation" OFF)

add_compile_options(-Wall -Wextra -pedantic -g -O3)
#add_compile_options(-save-temps)

# Enable pthread support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


# Find the NUMA library and headers
find_package(Numa QUIET) # Check for a configuration file (NumaConfig.cmake)

if (Numa_FOUND)
    message(STATUS "Found Numa library via find_package.")
    set(NUMA_INCLUDE_DIRS ${Numa_INCLUDE_DIRS})
    set(NUMA_LIBRARIES ${Numa_LIBRARIES})
else()
    # Fallback to manual search for library and header (robust approach)
    find_path(NUMA_INCLUDE_DIR numa.h
        HINTS /usr/include /usr/local/include
        PATHS /usr/include/numa
    )

    find_library(NUMA_LIBRARY numa
        HINTS /usr/lib /usr/local/lib
    )

    if (NUMA_INCLUDE_DIR AND NUMA_LIBRARY)
        message(STATUS "Found NUMA library and headers manually." ${NUMA_INCLUDE_DIR} -- ${NUMA_LIBRARY})
        set(NUMA_INCLUDE_DIRS ${NUMA_INCLUDE_DIR})
        set(NUMA_LIBRARIES ${NUMA_LIBRARY})
    else()
        message(WARNING "NUMA library/headers not found. Compilation will likely fail if NUMA code is enabled.")
        set(NUMA_INCLUDE_DIRS "")
        set(NUMA_LIBRARIES "")
    endif()
endif()

# Since we are using NUMA-aware code, we define a compile definition
if (NUMA_LIBRARIES)
    add_compile_definitions(HAS_NUMA_SUPPORT)
endif()

if(LOCKS AND RW_LOCKS)
    message(FATAL_ERROR "LOCKS and RW_LOCKS options are mutually exclusive. Please enable only one.")
endif()


if(LOCKS)
    set(FCDS OFF)
    set(CONC_MINHASH OFF)
    add_compile_definitions(LOCKS)
    message(STATUS "Using LOCKS-based sketch implementation.")
elseif(RW_LOCKS)
    set(FCDS OFF)
    set(CONC_MINHASH OFF)
    add_compile_definitions(RW_LOCKS)
    message(STATUS "Using RW_LOCKS-based sketch implementation.")
elseif(FCDS)
    set(LOCKS OFF)
    set(RW_LOCKS OFF)
    set(CONC_MINHASH OFF)
    add_compile_definitions(FCDS)
    message(STATUS "Using FCDS sketch implementation.")
else()
    set(LOCKS OFF)
    set(RW_LOCKS OFF)
    set(FCDS OFF)
    add_compile_definitions(CONC_MINHASH)
    set(CONC_MINHASH ON CACHE BOOL "Enable CONCURRENT MINHASH sketch implementation" FORCE)
    message(STATUS "Using our CONCURRENT MINHASH sketch implementation. ${CONC_MINHASH}")
endif()


include_directories(include)

add_subdirectory(src)
add_subdirectory(test)
