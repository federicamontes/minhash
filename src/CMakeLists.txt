set(minhashcore_srcs
    serial/minhash-serial.c
    utils/hash.c
    configuration/configuration.c
)


add_library(minhashcore STATIC ${minhashcore_srcs})


target_include_directories(minhashcore PRIVATE ${CMAKE_SOURCE_DIR}/include)
