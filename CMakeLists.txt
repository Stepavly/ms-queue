cmake_minimum_required(VERSION 3.20)
project(ms_queue)

find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover -Wextra -Wall")
set(CMAKE_CXX_STANDARD 17)

add_executable(tests sc_test.cpp ms_queue.h)
target_link_libraries(tests gtest gtest_main)
target_link_libraries(tests Threads::Threads)
add_subdirectory(lib/googletest)
