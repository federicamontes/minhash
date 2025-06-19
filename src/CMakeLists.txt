set(minhashcore_srcs
    serial/minhash-serial.c
    configuration/configuration.c
    parallel/minhash-parallel-lock.c
    utils/hash.c
    utils/utils.c
    
    parallel/minhash-fcds.c
)


add_library(minhashcore STATIC ${minhashcore_srcs})


target_include_directories(minhashcore PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(LOCKS OR RW_LOCKS OR FCDS)
  target_link_libraries(minhashcore PRIVATE Threads::Threads)
endif()
