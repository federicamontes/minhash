
# Enable CTest testing support
enable_testing()


add_executable(test_serial test_serial.c)
target_link_libraries(test_serial PRIVATE minhashcore)
target_include_directories(test_serial PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_serial_simil test_serial_similarity.c)
target_link_libraries(test_serial_simil PRIVATE minhashcore)
target_include_directories(test_serial_simil PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(LOCKS OR RW_LOCKS)
    add_executable(test_parallel_lock test_parallel_lock.c)
    target_link_libraries(test_parallel_lock PRIVATE minhashcore)
    target_include_directories(test_parallel_lock PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()


if(FCDS)
    add_executable(test_fcds test_fcds.c)
    target_link_libraries(test_fcds PRIVATE minhashcore)
    target_include_directories(test_fcds PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

if(CONC_MINHASH) #CONC_MINHASH
    add_executable(test_conc_minhash test_conc_minhash.c)
    target_link_libraries(test_conc_minhash PRIVATE minhashcore)
    target_include_directories(test_conc_minhash PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

# Always available tests
add_test(NAME test_serial COMMAND test_serial 100000 100 1)
add_test(NAME test_serial_simil COMMAND test_serial_simil 100000 100 1)

if(LOCKS OR RW_LOCKS)
    add_test(NAME test_parallel_lock COMMAND test_parallel_lock 100000 100 1 2)
endif()

if(FCDS)
    add_test(NAME test_fcds COMMAND test_fcds 100000 100 1 2 1000 1)
    add_test(NAME test_fcds2 COMMAND test_fcds 100000 100 1 11 1000 2)
endif()
