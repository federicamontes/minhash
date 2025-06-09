cmake_minimum_required(VERSION 3.16)
project(concurrent_minhash C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

add_subdirectory(src)
add_subdirectory(test)
