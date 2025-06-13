add_executable(test_serial test_serial.c)
add_executable(test_serial_simil test_serial_similarity.c)

if(LOCKS OR RW_LOCKS)
add_executable(test_parallel_lock test_parallel_lock.c)
endif()


target_link_libraries(test_serial PRIVATE minhashcore)
target_include_directories(test_serial PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_serial_simil PRIVATE minhashcore)
target_include_directories(test_serial_simil PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if(LOCKS OR RW_LOCKS)
target_link_libraries(test_parallel_lock PRIVATE minhashcore)
target_include_directories(test_parallel_lock PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
endif()