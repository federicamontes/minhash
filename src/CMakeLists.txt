enable_testing()


set(minhashcore_srcs
    serial/minhash-serial.c
    configuration/configuration.c
    utils/hash.c
    utils/utils.c
    
)

if(LOCKS OR RW_LOCKS)
  set(minhashcore_srcs ${minhashcore_srcs}
      parallel/minhash-parallel-lock.c

    )
elseif(FCDS)
  set(minhashcore_srcs ${minhashcore_srcs}
      fcds/minhash-fcds.c
      datatypes/sketch_list.c
    )
else()
  message(STATUS "Source CONCURRENT MINHASH sketch implementation.")
  set(minhashcore_srcs ${minhashcore_srcs}
      parallel/minhash-concurrent.c
      datatypes/linked_list.c
  )
endif()

add_library(minhashcore STATIC ${minhashcore_srcs})


target_include_directories(minhashcore PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(LOCKS OR RW_LOCKS OR FCDS OR CONC_MINHASH)
  target_link_libraries(minhashcore PRIVATE Threads::Threads atomic)
endif()
