cmake_minimum_required(VERSION 2.8)
project(jsnn)
enable_testing()

add_library(jsnn STATIC jsnn.c)

add_executable(jsnn_test jsnn_test.c)
target_link_libraries(jsnn_test jsnn)
set_target_properties(jsnn_test PROPERTIES COMPILE_FLAGS "-g")

add_test(jsnn_test "${EXECUTABLE_OUTPUT_PATH}/jsnn_test")

add_executable(jsnn_example example.c)
target_link_libraries(jsnn_example jsnn)
set_target_properties(jsnn_example PROPERTIES COMPILE_FLAGS "-g")
