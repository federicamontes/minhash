
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
elseif(FCDS)
    add_executable(test_fcds fcds/test_fcds.c)
    add_executable(test_fcds_wronly fcds/test_only_writes.c)
    add_executable(test_fcds_fix_wr fcds/test_fixed_writes_infinite_query.c)
    add_executable(test_fcds_fix_qr fcds/test_fixed_queries_infinite_write.c)
    add_executable(test_fcds_prob fcds/test_fcds_prob_ops.c)

    target_link_libraries(test_fcds PRIVATE minhashcore)
    target_link_libraries(test_fcds_wronly PRIVATE minhashcore)
    target_link_libraries(test_fcds_fix_wr PRIVATE minhashcore)
    target_link_libraries(test_fcds_fix_qr PRIVATE minhashcore)
    target_link_libraries(test_fcds_prob PRIVATE minhashcore)

    target_include_directories(test_fcds PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_fcds_wronly PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_fcds_fix_wr PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_fcds_fix_qr PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_fcds_prob PRIVATE ${CMAKE_SOURCE_DIR}/include)

else() #CONC_MINHASH

    add_executable(test_conc_minhash parallel/test_conc_minhash.c)
    add_executable(test_conc_wronly parallel/test_only_writes.c)
    add_executable(test_conc_fix_wr parallel/test_fixed_writes_infinite_query.c)
    add_executable(test_conc_fix_qr parallel/test_fixed_queries_infinite_write.c)
    add_executable(test_conc_prob parallel/test_conc_prob_ops.c)

    target_link_libraries(test_conc_minhash PRIVATE minhashcore)
    target_link_libraries(test_conc_wronly PRIVATE minhashcore)
    target_link_libraries(test_conc_fix_wr PRIVATE minhashcore)
    target_link_libraries(test_conc_fix_qr PRIVATE minhashcore)
    target_link_libraries(test_conc_prob PRIVATE minhashcore)
    
    target_include_directories(test_conc_minhash PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_conc_wronly PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_conc_fix_wr PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_conc_fix_qr PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_include_directories(test_conc_prob PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

# Always available tests
add_test(NAME test_serial COMMAND test_serial 1000000 100 1)
add_test(NAME test_serial_simil COMMAND test_serial_simil 1000000 100 1)

if(LOCKS OR RW_LOCKS)
    add_test(NAME test_parallel_lock COMMAND test_parallel_lock 100000 100 1 2)
elseif(FCDS)
    add_test(NAME test_fcds COMMAND test_fcds 1000000 100 1 2 1000 0)
    add_test(NAME test_fcds2 COMMAND test_fcds 1000000 100 1 8 1000 0)
    add_test(NAME test_fcds3 COMMAND test_fcds 1000000 100 1 8 50 0)  

else() #CONC_MINHASH
    add_test(NAME test_conc_minhash_serial COMMAND test_conc_minhash 1000000 100 1 1 1000 0)
    add_test(NAME test_conc_minhash_parallel COMMAND test_conc_minhash 1000000 100 1 2 1000 0)
    add_test(NAME test_conc_minhash_parallel2 COMMAND test_conc_minhash 1000000 100 1 8 1000 0)
    add_test(NAME test_conc_minhash_parallel3 COMMAND test_conc_minhash 1000000 100 1 8 50 0)
endif()