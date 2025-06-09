add_executable(test_serial test_serial.c)


target_link_libraries(test_serial PRIVATE minhashcore)
target_include_directories(test_serial PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)